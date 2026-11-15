// MuProxy2.Hook.dll - Hook de send/recv/WSASend/WSARecv via IAT
// Envia pacotes capturados via named pipe local \\.\pipe\MuProxy2

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <dbghelp.h>
#include <psapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

static HANDLE g_Pipe = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_PipeLock;

static int (WINAPI* TrueSend)(SOCKET s, const char* buf, int len, int flags) = nullptr;
static int (WINAPI* TrueRecv)(SOCKET s, char* buf, int len, int flags) = nullptr;
static int (WINAPI* TrueWSASend)(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesSent, DWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* TrueWSARecv)(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesRecvd, LPDWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;

static void EnsurePipe()
{
    if (g_Pipe != INVALID_HANDLE_VALUE) return;
    g_Pipe = CreateFileA(
        "\\\\.\\pipe\\MuProxy2",
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (g_Pipe == INVALID_HANDLE_VALUE)
    {
        // named pipe ainda nao existe; tenta conectar novamente no proximo pacote
        Sleep(10);
    }
}

static void LogRaw(const char* dir, const char* data, int len)
{
    if (len <= 0 || !data) return;

    EnterCriticalSection(&g_PipeLock);
    EnsurePipe();
    if (g_Pipe == INVALID_HANDLE_VALUE)
    {
        LeaveCriticalSection(&g_PipeLock);
        return;
    }

    // Formato: "PKT DIR HEX\n"
    char pkt[4096];
    int pos = snprintf(pkt, sizeof(pkt), "PKT %s ", dir);
    if (pos > 0 && pos < (int)sizeof(pkt))
    {
        for (int i = 0; i < len && pos + 2 < (int)sizeof(pkt); ++i)
            pos += snprintf(pkt + pos, sizeof(pkt) - pos, "%02X", (unsigned char)data[i]);
        pkt[pos++] = '\n';
        DWORD written = 0;
        WriteFile(g_Pipe, pkt, pos, &written, nullptr);
    }
    LeaveCriticalSection(&g_PipeLock);
}

static int WINAPI HookSend(SOCKET s, const char* buf, int len, int flags)
{
    if (buf && len > 0) LogRaw("SEND", buf, len);
    return TrueSend(s, buf, len, flags);
}

static int WINAPI HookRecv(SOCKET s, char* buf, int len, int flags)
{
    int r = TrueRecv(s, buf, len, flags);
    if (r > 0) LogRaw("RECV", buf, r);
    return r;
}

static int WINAPI HookWSASend(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesSent, DWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (buffers)
    {
        for (DWORD i = 0; i < bufferCount; ++i)
        {
            if (buffers[i].buf && buffers[i].len > 0)
                LogRaw("SEND", buffers[i].buf, buffers[i].len);
        }
    }
    return TrueWSASend(s, buffers, bufferCount, numberOfBytesSent, flags, lpOverlapped, lpCompletionRoutine);
}

static int WINAPI HookWSARecv(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesRecvd, LPDWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    int r = TrueWSARecv(s, buffers, bufferCount, numberOfBytesRecvd, flags, lpOverlapped, lpCompletionRoutine);
    if (r == 0 && buffers && numberOfBytesRecvd)
    {
        for (DWORD i = 0; i < bufferCount; ++i)
        {
            if (buffers[i].buf && numberOfBytesRecvd[0] > 0)
                LogRaw("RECV", buffers[i].buf, numberOfBytesRecvd[0]);
        }
    }
    return r;
}

static bool HookIATForModule(HMODULE hMod, const char* funcName, void* hookFunc, void** originalFunc)
{
    if (!hMod) return false;

    ULONG size = 0;
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
        hMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
    if (!importDesc) return false;

    for (; importDesc->Name; ++importDesc)
    {
        const char* dllName = (const char*)((BYTE*)hMod + importDesc->Name);
        if (_stricmp(dllName, "ws2_32.dll") != 0) continue;

        PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((BYTE*)hMod + importDesc->FirstThunk);
        for (; thunk->u1.Function; ++thunk)
        {
            PROC* funcAddr = (PROC*)&thunk->u1.Function;
            PROC target = (PROC)GetProcAddress(GetModuleHandleA("ws2_32.dll"), funcName);
            if (!target) continue;
            if (*funcAddr == target)
            {
                DWORD oldProtect = 0;
                if (!VirtualProtect(funcAddr, sizeof(PROC), PAGE_READWRITE, &oldProtect))
                    return false;
                // salva o original apenas na primeira vez; se ja hookamos, mantem
                if (*originalFunc == nullptr) *originalFunc = (void*)*funcAddr;
                *funcAddr = (PROC)hookFunc;
                VirtualProtect(funcAddr, sizeof(PROC), oldProtect, &oldProtect);
                return true;
            }
        }
    }
    return false;
}

// Mantem compatibilidade com chamadas antigas
static bool HookIAT(const char* moduleName, const char* funcName, void* hookFunc, void** originalFunc)
{
    return HookIATForModule(GetModuleHandleA(moduleName), funcName, hookFunc, originalFunc);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&g_PipeLock);

        HMODULE hMain = GetModuleHandleA(NULL);
        char modName[MAX_PATH] = {};
        GetModuleFileNameA(hMain, modName, MAX_PATH);

        // Hookar a IAT de TODOS os modulos carregados no processo, nao so main.exe
        // porque o jogo pode chamar send/recv a partir de Main.dll ou outras DLLs
        HMODULE hMods[1024];
        DWORD needed;
        if (EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &needed))
        {
            int count = (int)(needed / sizeof(HMODULE));
            for (int i = 0; i < count; ++i)
            {
                char name[MAX_PATH] = {};
                GetModuleFileNameA(hMods[i], name, MAX_PATH);
                // pula a propria hook.dll para evitar auto-hook recursivo
                if (_stricmp(name, modName) == 0) continue;

                HookIATForModule(hMods[i], "send", HookSend, (void**)&TrueSend);
                HookIATForModule(hMods[i], "recv", HookRecv, (void**)&TrueRecv);
                HookIATForModule(hMods[i], "WSASend", HookWSASend, (void**)&TrueWSASend);
                HookIATForModule(hMods[i], "WSARecv", HookWSARecv, (void**)&TrueWSARecv);
            }
        }
    }
    return TRUE;
}
