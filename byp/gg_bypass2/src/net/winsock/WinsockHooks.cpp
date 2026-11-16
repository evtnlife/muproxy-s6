// net/winsock/WinsockHooks.cpp — Hooks inline nas exports do ws2_32.dll
//
// Extraído do PacketSniffer (Fase 6 do refactor reorganize-byp-modules).
//
// O tag de log continua "PACKETSNIFFER" para manter a continuidade dos logs.
#include "sdk.h"
#include <cstdio>
#include <cstring>
#include <map>

#include "WinsockHooks.h"
#include "Logger.h"
#include "MuCipher.h"
#include "StackTrace.h"
#include "GameServerConnection.h"
#include "C3Decoder.h"
#include "OriginalApi.h"

#pragma comment(lib, "ws2_32.lib")

namespace WinsockHooks {

// ============================================================================
// APIs originais (trampolines dos hooks)
// ============================================================================
static int (WINAPI* TrueSend)(SOCKET s, const char* buf, int len, int flags) = nullptr;
static int (WINAPI* TrueRecv)(SOCKET s, char* buf, int len, int flags) = nullptr;
static int (WINAPI* TrueWSASend)(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesSent, DWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* TrueWSARecv)(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesRecvd, LPDWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* TrueSendTo)(SOCKET s, const char* buf, int len, int flags,
    const sockaddr* to, int tolen) = nullptr;
static int (WINAPI* TrueRecvFrom)(SOCKET s, char* buf, int len, int flags,
    sockaddr* from, int* fromlen) = nullptr;
static int (WINAPI* TrueConnect)(SOCKET s, const sockaddr* name, int namelen) = nullptr;
static int (WINAPI* TrueWSAConnect)(SOCKET s, const sockaddr* name, int namelen,
    LPWSABUF callerData, LPWSABUF calleeData,
    LPQOS sqos, LPQOS gqos) = nullptr;

// Forward: completion routine wrapper (definida após HookWSARecv)
static void CALLBACK OverlappedRecvCompletion(DWORD dwError, DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

// ============================================================================
// Observadores (o PacketSniffer monta o log/reassembly a partir deles)
// ============================================================================
static PacketObserver g_PacketObserver = nullptr;
static SocketClosedObserver g_SocketClosedObserver = nullptr;

void SetPacketObserver(PacketObserver observer) {
    g_PacketObserver = observer;
}

void SetSocketClosedObserver(SocketClosedObserver observer) {
    g_SocketClosedObserver = observer;
}

// ============================================================================
// Socket do game server (rastreado no connect do jogo)
// ============================================================================

// ============================================================================
// Overlapped WSARecv completion routine hook
// O jogo usa WSARecv com completion routine (I/O overlapped). O HookWSARecv
// só captura quando r==0 (completa imediato), mas overlapped retorna
// WSA_IO_PENDING e os dados chegam via completion routine. Embrulhamos a
// completion routine para logar os dados recebidos (RECV).
// ============================================================================
struct OverlappedRecvCtx {
    SOCKET s;
    LPWSABUF buffers;
    DWORD bufferCount;
    LPWSAOVERLAPPED_COMPLETION_ROUTINE original;
};
static std::map<LPWSAOVERLAPPED, OverlappedRecvCtx> g_OverlappedRecvMap;
static CRITICAL_SECTION g_OverlappedLock;
static bool g_OverlappedInit = false;

// ============================================================================
// Hooks
// ============================================================================
static int WINAPI HookSend(SOCKET s, const char* buf, int len, int flags)
{
    if (buf && len > 0) {
        // Pula texto ASCII (comandos INJECT etc)
        if (buf[0] >= 0x20 && buf[0] < 0x7F) {
            return TrueSend(s, buf, len, flags);
        }

        // Fallback: se o socket do GS ainda nao foi capturado pelo connect,
        // captura aqui no primeiro send do jogo (ex: pacote de login)
        if (GameServerConnection::GetSocket() == INVALID_SOCKET) {
            int port = 0;
            if (GameServerConnection::GetSocketPort(s, &port) && GameServerConnection::IsGameServerPort(port)) {
                GameServerConnection::SetSocket(s);
                Logger::Log("PACKETSNIFFER", "Game server socket rastreado via SEND: %d (porta %d)", (int)s, port);
            }
        }

        // Pega EBP para stack trace completo
        uintptr_t* ebpFrame;
        __asm { mov ebpFrame, ebp }

        // ---- Decifra Layer-1 (XOR 0x36, sub 0x07) para mostrar plaintext no log ----
        // O jogo envia bytes cifrados (headers FC/FE), mas queremos ver C1/C2/C3/C4.
        // Aplica MuCipher::FullDecrypt (apenas Layer-1, sem XorFilter) em uma copia.
        int port = 0;
        bool isGS = GameServerConnection::GetSocketPort(s, &port) && GameServerConnection::IsGameServerPort(port);
        char decBuf[4096];
        const char* displayBuf = buf;
        int displayLen = len;
        if (isGS) {
            int copyLen = len < (int)sizeof(decBuf) ? len : (int)sizeof(decBuf);
            memcpy(decBuf, buf, copyLen);
            unsigned char xorKey = 0, subKey = 0;
            MuCipher::GetGameServerKeys(&xorKey, &subKey);
            MuCipher::FullDecrypt(decBuf, copyLen, xorKey, subKey);
            displayBuf = decBuf;
            displayLen = copyLen;
        }

        // Hex do pacote (decifrado se for game server)
        char hex[256];
        int hpos = 0;
        int blen = displayLen > 48 ? 48 : displayLen;
        for (int j = 0; j < blen; ++j)
            hpos += snprintf(hex + hpos, sizeof(hex) - hpos, "%02X ", (unsigned char)displayBuf[j]);

        // Header type legivel: C1/C2/C3/C4 ou "??"
        const char* hdrType = "??";
        if (displayLen > 0) {
            switch ((unsigned char)displayBuf[0]) {
                case 0xC1: hdrType = "C1"; break;
                case 0xC2: hdrType = "C2"; break;
                case 0xC3: hdrType = "C3"; break;
                case 0xC4: hdrType = "C4"; break;
            }
        }

        Logger::Log("PACKETSNIFFER", "SEND %s %d bytes [%s]",
                    hdrType, displayLen, hex);
        StackTrace::Log("SEND stack", ebpFrame);

        // Decifra C3/C4 com a nossa Decrypt (DecKey = inversa modular do EncKey).
        // O NativeDecrypt do jogo falha em C2S: o DecKey do Obj1 (0x8797778) e zero.
        if (isGS) {
            C3Decoder::LogDecoded(displayBuf, displayLen);
        }

        if (g_PacketObserver) g_PacketObserver("SEND", s, buf, len);
    }

    return TrueSend(s, buf, len, flags);
}

static int WINAPI HookRecv(SOCKET s, char* buf, int len, int flags)
{
    int r = TrueRecv(s, buf, len, flags);
    if (r > 0) {
        Logger::Log("PACKETSNIFFER", "recv() ret=%d socket=%d", r, (int)s);
        if (g_PacketObserver) g_PacketObserver("RECV", s, buf, r);
    }
    if (r == 0 && g_SocketClosedObserver) g_SocketClosedObserver(s); // conexao fechada
    return r;
}

static int WINAPI HookWSASend(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesSent, DWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Loga TODO pacote enviado com hex dump + stack trace completo
    if (buffers)
    {
        for (DWORD i = 0; i < bufferCount; ++i)
        {
            if (buffers[i].buf && buffers[i].len > 0)
            {
                // Hex dump completo
                char hex[1024];
                int pos = 0;
                int blen = buffers[i].len;
                if (blen > 256) blen = 256;
                for (int j = 0; j < blen && pos < (int)sizeof(hex) - 4; ++j)
                    pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", (unsigned char)buffers[i].buf[j]);
                Logger::Log("PACKETSNIFFER", "WSASend: %d bytes [%s]", buffers[i].len, hex);
                uintptr_t* callerEbp;
                __asm { mov callerEbp, ebp }
                StackTrace::Log("WSASend stack", callerEbp);
                if (g_PacketObserver) g_PacketObserver("SEND", s, buffers[i].buf, buffers[i].len);
            }
        }
    }
    return TrueWSASend(s, buffers, bufferCount, numberOfBytesSent, flags, lpOverlapped, lpCompletionRoutine);
}

static int WINAPI HookWSARecv(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesRecvd, LPDWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Embrulha a completion routine para capturar dados recebidos via overlapped I/O.
    // O jogo usa WSARecv overlapped: retorna WSA_IO_PENDING e os dados chegam
    // depois via completion routine. Sem este wrap, nenhum RECV é capturado.
    LPWSAOVERLAPPED_COMPLETION_ROUTINE wrapped = lpCompletionRoutine;
    if (lpCompletionRoutine && lpOverlapped) {
        if (!g_OverlappedInit) {
            InitializeCriticalSection(&g_OverlappedLock);
            g_OverlappedInit = true;
        }
        EnterCriticalSection(&g_OverlappedLock);
        OverlappedRecvCtx ctx;
        ctx.s = s;
        ctx.buffers = buffers;
        ctx.bufferCount = bufferCount;
        ctx.original = lpCompletionRoutine;
        g_OverlappedRecvMap[lpOverlapped] = ctx;
        LeaveCriticalSection(&g_OverlappedLock);
        wrapped = OverlappedRecvCompletion;
    }

    int r = TrueWSARecv(s, buffers, bufferCount, numberOfBytesRecvd, flags, lpOverlapped, wrapped);

    // Diagnóstico: qual mecanismo de recv o jogo usa?
    Logger::Log("PACKETSNIFFER", "WSARecv: ret=%d err=%d socket=%d overlapped=%p routine=%p",
                r, (r == SOCKET_ERROR) ? WSAGetLastError() : 0, (int)s, (void*)lpOverlapped, (void*)lpCompletionRoutine);

    // Completa imediato (r==0): loga direto e remove do map.
    if (r == 0 && buffers && numberOfBytesRecvd) {
        for (DWORD i = 0; i < bufferCount; ++i) {
            if (buffers[i].buf && numberOfBytesRecvd[i] > 0 && g_PacketObserver)
                g_PacketObserver("RECV", s, buffers[i].buf, numberOfBytesRecvd[i]);
        }
        if (lpOverlapped) {
            EnterCriticalSection(&g_OverlappedLock);
            g_OverlappedRecvMap.erase(lpOverlapped);
            LeaveCriticalSection(&g_OverlappedLock);
        }
    }
    if (r != 0 && WSAGetLastError() != WSA_IO_PENDING && g_SocketClosedObserver)
        g_SocketClosedObserver(s);
    return r;
}

// Completion routine wrapper: loga dados recebidos e chama a original.
static void CALLBACK OverlappedRecvCompletion(DWORD dwError, DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
    OverlappedRecvCtx ctx;
    bool found = false;
    if (lpOverlapped) {
        EnterCriticalSection(&g_OverlappedLock);
        auto it = g_OverlappedRecvMap.find(lpOverlapped);
        if (it != g_OverlappedRecvMap.end()) {
            ctx = it->second;
            g_OverlappedRecvMap.erase(it);
            found = true;
        }
        LeaveCriticalSection(&g_OverlappedLock);
    }

    if (found && dwError == 0 && cbTransferred > 0 && ctx.buffers) {
        DWORD remaining = cbTransferred;
        for (DWORD i = 0; i < ctx.bufferCount && remaining > 0; ++i) {
            if (ctx.buffers[i].buf && ctx.buffers[i].len > 0) {
                DWORD n = ctx.buffers[i].len < remaining ? ctx.buffers[i].len : remaining;
                if (g_PacketObserver) g_PacketObserver("RECV", ctx.s, ctx.buffers[i].buf, (int)n);
                remaining -= n;
            }
        }
    }

    if (found && ctx.original)
        ctx.original(dwError, cbTransferred, lpOverlapped, dwFlags);
}

static int WINAPI HookSendTo(SOCKET s, const char* buf, int len, int flags,
    const sockaddr* to, int tolen)
{
    if (buf && len > 0 && g_PacketObserver) g_PacketObserver("SEND", s, buf, len);
    return TrueSendTo(s, buf, len, flags, to, tolen);
}

static int WINAPI HookRecvFrom(SOCKET s, char* buf, int len, int flags,
    sockaddr* from, int* fromlen)
{
    int r = TrueRecvFrom(s, buf, len, flags, from, fromlen);
    if (r > 0 && g_PacketObserver) g_PacketObserver("RECV", s, buf, r);
    return r;
}

static int WINAPI HookConnect(SOCKET s, const sockaddr* name, int namelen)
{
    if (name && namelen >= sizeof(sockaddr_in))
    {
        sockaddr_in target = *(sockaddr_in*)name;
        char ip[16];
        strncpy_s(ip, inet_ntoa(target.sin_addr), 15);
        int port = ntohs(target.sin_port);
        Logger::Log("PACKETSNIFFER", "CONNECT to %s:%d", ip, port);

        // Rastreia socket do game server para injects
        if (GameServerConnection::IsGameServerPort(port))
        {
            GameServerConnection::SetSocket(s);
            Logger::Log("PACKETSNIFFER", "Game server socket rastreado: %d", (int)s);
        }

        target.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        return TrueConnect(s, (sockaddr*)&target, sizeof(target));
    }
    return TrueConnect(s, name, namelen);
}

static int WINAPI HookWSAConnect(SOCKET s, const sockaddr* name, int namelen,
    LPWSABUF callerData, LPWSABUF calleeData, LPQOS sqos, LPQOS gqos)
{
    if (name && namelen >= sizeof(sockaddr_in))
    {
        sockaddr_in target = *(sockaddr_in*)name;
        char ip[16];
        strncpy_s(ip, inet_ntoa(target.sin_addr), 15);
        int port = ntohs(target.sin_port);
        Logger::Log("PACKETSNIFFER", "WSACONNECT to %s:%d", ip, port);

        // Rastreia socket do game server para injects
        if (GameServerConnection::IsGameServerPort(port))
        {
            GameServerConnection::SetSocket(s);
            Logger::Log("PACKETSNIFFER", "Game server socket rastreado: %d", (int)s);
        }

        target.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        return TrueWSAConnect(s, (sockaddr*)&target, sizeof(target), callerData, calleeData, sqos, gqos);
    }
    return TrueWSAConnect(s, name, namelen, callerData, calleeData, sqos, gqos);
}

// ============================================================================
// Trampolines (inline hook x86)
// ============================================================================
#pragma pack(push, 1)
struct Trampoline {
    BYTE origBytes[5];
    BYTE jmp;
    DWORD rel;
};

struct InlineHookState {
    void* target;
    BYTE original[5];
    Trampoline* trampoline;
    bool installed;
};
#pragma pack(pop)

static InlineHookState g_Hooks[16];
static int g_HookCount = 0;

static Trampoline* AllocateTrampoline()
{
    return (Trampoline*)VirtualAlloc(nullptr, sizeof(Trampoline), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

static bool InstallInlineHook(void* target, void* hook, InlineHookState* state)
{
    if (!target || !hook || !state) return false;
    if (state->installed) return true;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        Logger::Log("PACKETSNIFFER", "VirtualProtect falhou no target %p", target);
        return false;
    }

    BYTE* t = (BYTE*)target;
    memcpy(state->original, t, 5);

    Trampoline* tramp = AllocateTrampoline();
    if (!tramp)
    {
        VirtualProtect(target, 5, oldProtect, &oldProtect);
        Logger::Log("PACKETSNIFFER", "Falha ao alocar trampoline");
        return false;
    }

    memcpy(tramp->origBytes, state->original, 5);
    tramp->jmp = 0xE9;
    tramp->rel = (DWORD)((t + 5) - ((BYTE*)tramp + 10));

    state->target = target;
    state->trampoline = tramp;
    state->installed = true;

    LONG rel = (LONG)((BYTE*)hook - (t + 5));
    t[0] = 0xE9;
    memcpy(t + 1, &rel, 4);

    VirtualProtect(target, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    FlushInstructionCache(GetCurrentProcess(), tramp, sizeof(Trampoline));
    return true;
}

static void* GetExportAddress(const char* name)
{
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) return nullptr;
    return GetProcAddress(hWs2, name);
}

// ============================================================================
// Install / Uninstall
// ============================================================================
bool Install()
{
    Logger::Log("PACKETSNIFFER", "Instalando inline hooks nas exports do ws2_32.dll");

    struct { const char* name; void* hook; void** original; } targets[] = {
        { "send", (void*)HookSend, (void**)&TrueSend },
        { "recv", (void*)HookRecv, (void**)&TrueRecv },
        { "WSASend", (void*)HookWSASend, (void**)&TrueWSASend },
        { "WSARecv", (void*)HookWSARecv, (void**)&TrueWSARecv },
        { "sendto", (void*)HookSendTo, (void**)&TrueSendTo },
        { "recvfrom", (void*)HookRecvFrom, (void**)&TrueRecvFrom },
        { "connect", (void*)HookConnect, (void**)&TrueConnect },
        { "WSAConnect", (void*)HookWSAConnect, (void**)&TrueWSAConnect },
    };

    for (int i = 0; i < 8; ++i)
    {
        void* target = GetExportAddress(targets[i].name);
        if (!target)
        {
            Logger::Log("PACKETSNIFFER", "%s nao encontrado no ws2_32", targets[i].name);
            continue;
        }

        if (InstallInlineHook(target, targets[i].hook, &g_Hooks[g_HookCount]))
        {
            *targets[i].original = g_Hooks[g_HookCount].trampoline;
            Logger::Log("PACKETSNIFFER", "Hook instalado em ws2_32.%s -> %p (trampoline=%p)",
                targets[i].name, target, *targets[i].original);
            g_HookCount++;
        }
        else
        {
            Logger::Log("PACKETSNIFFER", "FALHA ao hookar ws2_32.%s", targets[i].name);
        }
    }

    Logger::Log("PACKETSNIFFER", "Total de hooks instalados=%d", g_HookCount);

    // Expõe os trampolines (send/connect originais) para os canais TCP
    // internos, que não devem passar pelo hook.
    OriginalApi::Set(TrueSend, TrueConnect);

    return g_HookCount > 0;
}

void Uninstall()
{
    for (int i = 0; i < g_HookCount; ++i)
    {
        if (!g_Hooks[i].installed || !g_Hooks[i].target) continue;
        DWORD oldProtect = 0;
        if (VirtualProtect(g_Hooks[i].target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            memcpy(g_Hooks[i].target, g_Hooks[i].original, 5);
            VirtualProtect(g_Hooks[i].target, 5, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), g_Hooks[i].target, 5);
        }
        if (g_Hooks[i].trampoline)
        {
            VirtualFree(g_Hooks[i].trampoline, 0, MEM_RELEASE);
        }
        g_Hooks[i].installed = false;
    }
    g_HookCount = 0;

    GameServerConnection::SetSocket(INVALID_SOCKET);
}

} // namespace WinsockHooks
