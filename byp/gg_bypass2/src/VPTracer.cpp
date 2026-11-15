#include "include/VPTracer.h"
#include "include/Logger.h"
#include "include/PsBypass.h"
#include "include/sdk.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// VPTracer — rastreamento MINIMALISTA e seguro de VirtualProtect.
//
// Objetivo: descobrir quais regioes o PS.dll (des)protege, sem crashar.
//
// Design anti-recursao:
//   * Hook nao chama NENHUMA API extra dentro do caminho rapido.
//   * Apenas escreve em buffer circular lock-free.
//   * Thread separada consome o buffer e grava em arquivo.
//   * TLS flag garante que reentrancia chama original direto.
// ============================================================================

typedef BOOL (WINAPI *VirtualProtect_t)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);

static VirtualProtect_t g_OriginalVirtualProtect = nullptr;
static BYTE g_OriginalBytes[5] = {0};
static BYTE g_HookBytes[5] = {0};
static volatile LONG g_HookInstalled = 0;

struct VPEvent {
    uintptr_t address;
    SIZE_T    size;
    DWORD     newProtect;
    DWORD     oldProtect;
    uintptr_t caller;
    DWORD     threadId;
    DWORD     fromPsDll;
};

static const SIZE_T VP_BUFFER_SIZE = 65536;
static VPEvent g_VpBuffer[VP_BUFFER_SIZE];
static volatile LONG g_VpHead = 0;
static volatile LONG g_VpTail = 0;
static volatile LONG g_LoggerRunning = 0;
static HANDLE g_LoggerThread = nullptr;

static const char* ProtectName(DWORD protect) {
    switch (protect) {
        case PAGE_EXECUTE:           return "EXEC";
        case PAGE_EXECUTE_READ:      return "EXEC_READ";
        case PAGE_EXECUTE_READWRITE: return "EXEC_READWRITE";
        case PAGE_EXECUTE_WRITECOPY: return "EXEC_WRITECOPY";
        case PAGE_NOACCESS:          return "NOACCESS";
        case PAGE_READONLY:          return "READONLY";
        case PAGE_READWRITE:         return "READWRITE";
        case PAGE_WRITECOPY:         return "WRITECOPY";
        default:                     return "UNKNOWN";
    }
}

static bool IsAddressInPsDll(uintptr_t addr) {
    HMODULE hPs = PsBypass::GetPsModule();
    if (!hPs) return false;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hPs;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hPs + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    uintptr_t base = (uintptr_t)hPs;
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    return addr >= base && addr < end;
}

static DWORD WINAPI LoggerThread(LPVOID) {
    char logPath[MAX_PATH] = {0};
    HMODULE hMod = GetModuleHandleA(NULL);
    if (hMod && GetModuleFileNameA(hMod, logPath, MAX_PATH)) {
        char* slash = strrchr(logPath, '\\');
        if (slash) *(slash + 1) = '\0';
        strcat_s(logPath, sizeof(logPath), "vptrace.log");
    } else {
        strcpy_s(logPath, sizeof(logPath), "vptrace.log");
    }

    HANDLE hFile = CreateFileA(logPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        g_LoggerRunning = 0;
        return 1;
    }

    DWORD written = 0;
    const char* header = "=== VPTrace iniciado ===\r\n";
    WriteFile(hFile, header, (DWORD)strlen(header), &written, NULL);

    while (g_LoggerRunning) {
        LONG head = g_VpHead;
        LONG tail = g_VpTail;
        LONG count = head - tail;
        if (count < 0) count += VP_BUFFER_SIZE * 2;

        if (count == 0) {
            Sleep(50);
            continue;
        }

        for (LONG i = 0; i < count && i < 256; i++) {
            LONG idx = (tail + i) % VP_BUFFER_SIZE;
            const VPEvent& e = g_VpBuffer[idx];
            char line[256];
            sprintf_s(line, sizeof(line),
                "%s tid=%08X addr=%p size=%08X new=%s(0x%X) old=%s(0x%X) caller=%p\r\n",
                e.fromPsDll ? "[PS]" : "[  ]",
                e.threadId,
                (LPVOID)e.address,
                (DWORD)e.size,
                ProtectName(e.newProtect), e.newProtect,
                ProtectName(e.oldProtect), e.oldProtect,
                (LPVOID)e.caller);
            WriteFile(hFile, line, (DWORD)strlen(line), &written, NULL);
        }
        g_VpTail = (tail + count) % VP_BUFFER_SIZE;
    }

    LONG head = g_VpHead;
    LONG tail = g_VpTail;
    LONG count = head - tail;
    if (count < 0) count += VP_BUFFER_SIZE * 2;
    for (LONG i = 0; i < count; i++) {
        LONG idx = (tail + i) % VP_BUFFER_SIZE;
        const VPEvent& e = g_VpBuffer[idx];
        char line[256];
        sprintf_s(line, sizeof(line),
            "%s tid=%08X addr=%p size=%08X new=%s(0x%X) old=%s(0x%X) caller=%p\r\n",
            e.fromPsDll ? "[PS]" : "[  ]",
            e.threadId,
            (LPVOID)e.address,
            (DWORD)e.size,
            ProtectName(e.newProtect), e.newProtect,
            ProtectName(e.oldProtect), e.oldProtect,
            (LPVOID)e.caller);
        DWORD written2 = 0;
        WriteFile(hFile, line, (DWORD)strlen(line), &written2, NULL);
    }

    CloseHandle(hFile);
    return 0;
}

static __declspec(thread) int g_InHook = 0;

static BOOL WINAPI HookedVirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect) {
    if (g_InHook) {
        return g_OriginalVirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect);
    }
    g_InHook = 1;

    uintptr_t caller = 0;
    __asm {
        mov eax, [ebp+4]
        mov caller, eax
    }

    DWORD oldProtect = 0;
    BOOL result = g_OriginalVirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect);
    if (lpflOldProtect) oldProtect = *lpflOldProtect;

    bool fromPs = IsAddressInPsDll(caller);

    LONG next = InterlockedIncrement(&g_VpHead);
    LONG idx = (next - 1) % VP_BUFFER_SIZE;
    VPEvent& e = g_VpBuffer[idx];
    e.address = (uintptr_t)lpAddress;
    e.size = dwSize;
    e.newProtect = flNewProtect;
    e.oldProtect = oldProtect;
    e.caller = caller;
    e.threadId = GetCurrentThreadId();
    e.fromPsDll = fromPs ? 1 : 0;

    g_InHook = 0;
    return result;
}

namespace VPTracer {

bool Install() {
    if (InterlockedCompareExchange(&g_HookInstalled, 1, 0) != 0) {
        return true;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) return false;

    FARPROC p = GetProcAddress(hKernel32, "VirtualProtect");
    if (!p) return false;

    g_OriginalVirtualProtect = (VirtualProtect_t)p;

    DWORD oldProtect;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_OriginalVirtualProtect = nullptr;
        InterlockedExchange(&g_HookInstalled, 0);
        return false;
    }

    memcpy(g_OriginalBytes, p, 5);
    DWORD rel = (DWORD)((uintptr_t)HookedVirtualProtect - (uintptr_t)p - 5);
    g_HookBytes[0] = 0xE9;
    *(DWORD*)(g_HookBytes + 1) = rel;
    memcpy(p, g_HookBytes, 5);

    VirtualProtect(p, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), p, 5);

    InterlockedExchange(&g_LoggerRunning, 1);
    g_LoggerThread = CreateThread(NULL, 0, LoggerThread, NULL, 0, NULL);

    Logger::Log("VPTRACE", "VirtualProtect hook minimalista instalado (buffer=%zu eventos)", VP_BUFFER_SIZE);
    return true;
}

void Uninstall() {
    if (!g_OriginalVirtualProtect || InterlockedExchange(&g_HookInstalled, 0) != 1) {
        return;
    }

    InterlockedExchange(&g_LoggerRunning, 0);
    if (g_LoggerThread) {
        WaitForSingleObject(g_LoggerThread, 2000);
        CloseHandle(g_LoggerThread);
        g_LoggerThread = nullptr;
    }

    FARPROC p = (FARPROC)g_OriginalVirtualProtect;
    DWORD oldProtect;
    if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(p, g_OriginalBytes, 5);
        VirtualProtect(p, 5, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), p, 5);
    }

    g_OriginalVirtualProtect = nullptr;
    Logger::Log("VPTRACE", "VirtualProtect hook removido");
}

} // namespace VPTracer
