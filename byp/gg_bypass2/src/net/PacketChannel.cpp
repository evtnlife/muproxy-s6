// PacketChannel.cpp — Canal de PACOTES com o MuProxy2 (TCP 44413)
//
// Extraído do PacketSniffer (Fase 5 do refactor reorganize-byp-modules):
// o socket do canal de pacotes (44413) é dono deste módulo.
//
// O tag de log continua "PACKETSNIFFER" para manter a continuidade dos logs.
#include "sdk.h"
#include <cstdio>
#include <cstring>

#include "PacketChannel.h"
#include "OriginalApi.h"
#include "PacketInjector.h"
#include "Config.h"
#include "Logger.h"

#pragma comment(lib, "ws2_32.lib")

namespace PacketChannel {

static SOCKET g_Socket = INVALID_SOCKET;
static CRITICAL_SECTION g_Lock;
static HANDLE g_ConnectThread = NULL;
static HANDLE g_RecvThread = NULL;
static volatile LONG g_Running = 0;
static bool g_Initialized = false;
static bool g_WinsockStarted = false;

static void EnsureWinsockStarted()
{
    if (!g_WinsockStarted)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) g_WinsockStarted = true;
    }
}

static void CloseSocket()
{
    if (g_Socket != INVALID_SOCKET)
    {
        closesocket(g_Socket);
        g_Socket = INVALID_SOCKET;
    }
}

static bool OpenSocket()
{
    if (g_Socket != INVALID_SOCKET) return true;
    EnsureWinsockStarted();

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)Config::PACKET_SNIFFER_TCP_PORT);
    inet_pton(AF_INET, Config::PACKET_SNIFFER_TCP_HOST, &addr.sin_addr);

    // Connect ORIGINAL (fora do hook): o hook reescreve o endereço e loga a
    // cada tentativa — a thread de connect tenta de novo a cada segundo.
    OriginalApi::ConnectFn origConnect = OriginalApi::GetConnect();
    int result = origConnect ? origConnect(s, (sockaddr*)&addr, sizeof(addr))
                              : connect(s, (sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        closesocket(s);
        // Log com throttle (10s): a thread de connect tenta a cada 1s, sem isso
        // o log encheria durante o tempo que o MuProxy2 nao estiver ouvindo.
        static DWORD lastFailLog = 0;
        DWORD now = GetTickCount();
        if (now - lastFailLog > 10000)
        {
            lastFailLog = now;
            Logger::Log("PACKETSNIFFER", "Falha ao conectar %s:%d (err=%d)",
                        Config::PACKET_SNIFFER_TCP_HOST, Config::PACKET_SNIFFER_TCP_PORT, err);
        }
        return false;
    }

    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
    g_Socket = s;
    Logger::Log("PACKETSNIFFER", "Conectado ao MuProxy2 via TCP %s:%d",
                Config::PACKET_SNIFFER_TCP_HOST, Config::PACKET_SNIFFER_TCP_PORT);
    return true;
}

static DWORD WINAPI ConnectThread(LPVOID)
{
    while (g_Running)
    {
        EnterCriticalSection(&g_Lock);
        if (g_Socket == INVALID_SOCKET) OpenSocket();
        LeaveCriticalSection(&g_Lock);
        Sleep(1000);
    }
    return 0;
}

// ============================================================================
// Receive thread: le comandos do MuProxy2 (SEND <hex>)
// ============================================================================
static DWORD WINAPI RecvThread(LPVOID)
{
    char buf[8192];
    int bufPos = 0;

    while (g_Running)
    {
        EnterCriticalSection(&g_Lock);
        bool connected = (g_Socket != INVALID_SOCKET);
        SOCKET s = g_Socket;
        LeaveCriticalSection(&g_Lock);

        if (!connected) { Sleep(500); continue; }

        // Usa select() para esperar dados sem busy-wait
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);
        timeval timeout = { 0, 500000 }; // 500ms
        int sel = select(0, &readSet, NULL, NULL, &timeout);
        if (sel <= 0) continue;

        // Le dados disponiveis
        char tmp[4096];
        int r = recv(s, tmp, sizeof(tmp) - 1, 0);
        if (r == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) continue;
            Logger::Log("PACKETSNIFFER", "TcpRecvThread: recv error %d", err);
            EnterCriticalSection(&g_Lock);
            CloseSocket();
            LeaveCriticalSection(&g_Lock);
            continue;
        }
        if (r == 0)
        {
            Logger::Log("PACKETSNIFFER", "TcpRecvThread: conexao fechada");
            EnterCriticalSection(&g_Lock);
            CloseSocket();
            LeaveCriticalSection(&g_Lock);
            continue;
        }
        tmp[r] = '\0';

        // Acumula no buffer e processa linhas completas
        for (int i = 0; i < r; i++)
        {
            if (tmp[i] == '\n')
            {
                buf[bufPos] = '\0';
                if (bufPos > 0)
                {
                    // Comando SEND [dir] <hex>  (dir opcional, ignorado - sempre injeta C->S)
                    if (strncmp(buf, "SEND ", 5) == 0)
                    {
                        char* args = buf + 5;
                        // Pula token opcional de direcao
                        char* hex = args;
                        if (strncmp(args, "Client ", 7) == 0 ||
                            strncmp(args, "Server ", 7) == 0 ||
                            strncmp(args, "C->S ", 5) == 0 ||
                            strncmp(args, "S->C ", 5) == 0)
                        {
                            // Avanca ate o proximo espaco
                            while (*hex && *hex != ' ') hex++;
                            while (*hex == ' ') hex++;
                        }

                        int hexLen = (int)strlen(hex);
                        BYTE packet[2048];
                        int packetLen = 0;
                        for (int j = 0; j < hexLen; j += 2)
                        {
                            if (hex[j] == ' ' || hex[j] == '\r' || hex[j] == '\0') { j--; continue; }
                            if (hex[j+1] == '\0') break;
                            unsigned int byteVal;
                            char tmpHex[3] = { hex[j], hex[j+1], 0 };
                            if (sscanf_s(tmpHex, "%02X", &byteVal) == 1)
                                packet[packetLen++] = (BYTE)byteVal;
                        }

                        if (packetLen > 0)
                        {
                            PacketInjector::InjectXorData(packet, packetLen);
                        }
                    }
                }
                bufPos = 0;
            }
            else if (bufPos < (int)sizeof(buf) - 1)
            {
                buf[bufPos++] = tmp[i];
            }
        }
    }
    return 0;
}

bool SendLine(const char* line, int len)
{
    if (!line || len <= 0) return false;

    EnterCriticalSection(&g_Lock);
    if (!OpenSocket())
    {
        LeaveCriticalSection(&g_Lock);
        return false;
    }

    int sent = 0;
    OriginalApi::SendFn origSend = OriginalApi::GetSend();
    while (sent < len)
    {
        int r = origSend ? origSend(g_Socket, line + sent, len - sent, 0)
                         : send(g_Socket, line + sent, len - sent, 0);
        if (r == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) { Sleep(1); continue; }
            CloseSocket();
            LeaveCriticalSection(&g_Lock);
            return false;
        }
        sent += r;
    }
    LeaveCriticalSection(&g_Lock);
    return true;
}

bool IsChannelSocket(SOCKET s)
{
    if (s == INVALID_SOCKET || !g_Initialized) return false;
    EnterCriticalSection(&g_Lock);
    bool same = (s == g_Socket);
    LeaveCriticalSection(&g_Lock);
    return same;
}

bool Initialize()
{
    if (g_Initialized) return true;
    InitializeCriticalSection(&g_Lock);
    g_Initialized = true;
    InterlockedExchange(&g_Running, 1);

    g_ConnectThread = CreateThread(NULL, 0, ConnectThread, NULL, 0, NULL);
    g_RecvThread = CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);

    Logger::Log("PACKETSNIFFER", "Canal de pacotes iniciado (%s:%d)",
                Config::PACKET_SNIFFER_TCP_HOST, Config::PACKET_SNIFFER_TCP_PORT);
    return true;
}

void Shutdown()
{
    InterlockedExchange(&g_Running, 0);

    if (g_ConnectThread)
    {
        WaitForSingleObject(g_ConnectThread, 1000);
        CloseHandle(g_ConnectThread);
        g_ConnectThread = NULL;
    }
    if (g_RecvThread)
    {
        WaitForSingleObject(g_RecvThread, 1000);
        CloseHandle(g_RecvThread);
        g_RecvThread = NULL;
    }

    EnterCriticalSection(&g_Lock);
    CloseSocket();
    LeaveCriticalSection(&g_Lock);

    if (g_Initialized)
    {
        DeleteCriticalSection(&g_Lock);
        g_Initialized = false;
    }
}

} // namespace PacketChannel
