// CommandChannel.cpp — Canal de COMANDOS com o MuProxy2 (TCP 44412)
//
// Extraído do antigo TcpClient (Fase 5 do refactor reorganize-byp-modules).
// O tag de log continua "TCPCLIENT" para manter a continuidade dos logs.
#include "sdk.h"
#include <mstcpip.h>
#include <cstdio>
#include <cstdarg>

#include "CommandChannel.h"
#include "OriginalApi.h"
#include "PacketHandler.h"
#include "Logger.h"

#pragma comment(lib, "ws2_32.lib")

namespace CommandChannel {

static const char* g_Host = "127.0.0.1";
static int g_Port = 44412;
static SOCKET g_Socket = INVALID_SOCKET;
static CRITICAL_SECTION g_Lock;
static bool g_Initialized = false;
static bool g_WinsockStarted = false;
static HANDLE g_WorkerThread = NULL;
static volatile LONG g_Running = 0;

static void EnsureWinsock() {
    if (!g_WinsockStarted) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
            g_WinsockStarted = true;
    }
}

static void CloseSocket() {
    if (g_Socket != INVALID_SOCKET) {
        closesocket(g_Socket);
        g_Socket = INVALID_SOCKET;
    }
}

static bool DoConnect() {
    EnsureWinsock();
    if (g_Socket != INVALID_SOCKET) return true;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)g_Port);
    inet_pton(AF_INET, g_Host, &addr.sin_addr);

    // Usa o connect ORIGINAL (fora do hook) — o hook de connect reescreve o
    // endereço e loga a cada tentativa, poluindo o log.
    OriginalApi::ConnectFn origConnect = OriginalApi::GetConnect();
    int result = origConnect ? origConnect(s, (sockaddr*)&addr, sizeof(addr))
                              : connect(s, (sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        Logger::Log("TCPCLIENT", "connect falhou para %s:%d (err=%d)", g_Host, g_Port, err);
        closesocket(s);
        return false;
    }

    // Non-blocking para recv thread
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    // Keepalive: detecta conexao morta rapidamente
    DWORD keepAlive = 1;
    tcp_keepalive ka = {};
    ka.onoff = 1;
    ka.keepalivetime = 5000;   // 5s sem dados -> inicia probes
    ka.keepaliveinterval = 1000; // 1s entre probes
    DWORD retBytes = 0;
    WSAIoctl(s, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), NULL, 0, &retBytes, NULL, NULL);
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));

    g_Socket = s;
    Logger::Log("TCPCLIENT", "Conectado ao %s:%d (socket=%d)", g_Host, g_Port, (int)s);
    return true;
}

// Thread unificada: mantém conexão ativa e recebe comandos do MuProxy2
static DWORD WINAPI WorkerThread(LPVOID) {
    char buf[8192];
    int bufPos = 0;

    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        EnterCriticalSection(&g_Lock);
        if (g_Socket == INVALID_SOCKET) {
            DoConnect();
        }
        SOCKET s = g_Socket;
        LeaveCriticalSection(&g_Lock);

        if (s == INVALID_SOCKET) {
            Sleep(1000); // aguarda antes de tentar reconectar
            continue;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);
        timeval timeout = { 0, 100000 }; // 100ms
        int sel = select(0, &readSet, NULL, NULL, &timeout);
        if (sel <= 0) continue;

        char tmp[4096];
        int r = recv(s, tmp, sizeof(tmp) - 1, 0);
        if (r == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) continue;
            Logger::Log("TCPCLIENT", "WorkerThread: recv error %d - fechando socket", err);
            EnterCriticalSection(&g_Lock);
            CloseSocket();
            LeaveCriticalSection(&g_Lock);
            continue;
        }
        if (r == 0) {
            Logger::Log("TCPCLIENT", "WorkerThread: conexao fechada pelo peer - fechando socket");
            EnterCriticalSection(&g_Lock);
            CloseSocket();
            LeaveCriticalSection(&g_Lock);
            continue;
        }
        tmp[r] = '\0';

        // Strip BOM (EF BB BF) se presente no inicio do stream
        static bool bomStripped = false;

        for (int i = 0; i < r; i++) {
            // Pula BOM bytes no inicio
            if (!bomStripped) {
                if ((unsigned char)tmp[i] == 0xEF || (unsigned char)tmp[i] == 0xBB || (unsigned char)tmp[i] == 0xBF) {
                    continue; // pula byte do BOM
                }
                bomStripped = true;
            }
            if (tmp[i] == '\n') {
                buf[bufPos] = '\0';
                if (bufPos > 0) {
                    PacketHandler::ProcessLine(buf, bufPos);
                }
                bufPos = 0;
            } else if (bufPos < (int)sizeof(buf) - 1) {
                buf[bufPos++] = tmp[i];
            }
        }
    }
    return 0;
}

bool Initialize(const char* host, int port) {
    if (g_Initialized) return true;
    g_Host = host;
    g_Port = port;
    InitializeCriticalSection(&g_Lock);
    g_Initialized = true;

    InterlockedExchange(&g_Running, 1);
    g_WorkerThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    return true;
}

void Shutdown() {
    if (!g_Initialized) return;
    InterlockedExchange(&g_Running, 0);
    if (g_WorkerThread) {
        WaitForSingleObject(g_WorkerThread, 1000);
        CloseHandle(g_WorkerThread);
        g_WorkerThread = NULL;
    }
    EnterCriticalSection(&g_Lock);
    CloseSocket();
    LeaveCriticalSection(&g_Lock);
    DeleteCriticalSection(&g_Lock);
    g_Initialized = false;
}

bool Send(const char* data, int len) {
    if (!g_Initialized || len <= 0) return false;

    EnterCriticalSection(&g_Lock);
    if (g_Socket == INVALID_SOCKET) {
        if (!DoConnect()) {
            LeaveCriticalSection(&g_Lock);
            return false;
        }
    }

    int sent = 0;
    OriginalApi::SendFn origSend = OriginalApi::GetSend();
    while (sent < len) {
        int r = origSend ? origSend(g_Socket, data + sent, len - sent, 0)
                         : send(g_Socket, data + sent, len - sent, 0);
        if (r == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) { Sleep(1); continue; }
            Logger::Log("TCPCLIENT", "Send: send error %d - fechando socket", err);
            CloseSocket();
            LeaveCriticalSection(&g_Lock);
            return false;
        }
        sent += r;
    }
    LeaveCriticalSection(&g_Lock);
    return true;
}

bool SendLine(const char* line, int len) {
    if (!line || len <= 0) return false;
    char buf[8192];
    int copyLen = len < (int)sizeof(buf) - 1 ? len : (int)sizeof(buf) - 1;
    memcpy(buf, line, copyLen);
    if (copyLen > 0 && buf[copyLen - 1] != '\n')
        buf[copyLen++] = '\n';
    return Send(buf, copyLen);
}

bool SendFormat(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len <= 0 || len >= (int)sizeof(buf)) return false;
    return SendLine(buf, len);
}

SOCKET GetSocket() {
    if (!g_Initialized) return INVALID_SOCKET;
    EnterCriticalSection(&g_Lock);
    SOCKET s = g_Socket;
    LeaveCriticalSection(&g_Lock);
    return s;
}

bool IsChannelSocket(SOCKET s) {
    if (s == INVALID_SOCKET || !g_Initialized) return false;
    EnterCriticalSection(&g_Lock);
    bool same = (s == g_Socket);
    LeaveCriticalSection(&g_Lock);
    return same;
}

bool IsConnected() {
    if (!g_Initialized) return false;
    EnterCriticalSection(&g_Lock);
    bool connected = (g_Socket != INVALID_SOCKET);
    LeaveCriticalSection(&g_Lock);
    return connected;
}

void Reconnect() {
    if (!g_Initialized) return;
    EnterCriticalSection(&g_Lock);
    CloseSocket();
    DoConnect();
    LeaveCriticalSection(&g_Lock);
}

} // namespace CommandChannel