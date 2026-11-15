#include "include/StartupHook.h"
#include "include/Logger.h"
#include "include/PsBypass.h"
#include "include/sdk.h"
#include <stdint.h>

typedef VOID (WINAPI *PGETSTARTUPINFOA)(LPSTARTUPINFOA);
static PGETSTARTUPINFOA pOriginalGetStartupInfoA = NULL;
BYTE StartupHook::g_OriginalGetStartupInfoA[5] = {0x8B, 0xFF, 0x55, 0x8B, 0xEC};

static BYTE g_HookBytes[5];
static volatile LONG g_HookTriggered = 0;
static volatile LONG g_HookInstalled = 0;

static VOID WINAPI HookedGetStartupInfoA(LPSTARTUPINFOA lpStartupInfo) {
    if (InterlockedCompareExchange(&g_HookTriggered, 1, 0) != 0) {
        pOriginalGetStartupInfoA(lpStartupInfo);
        return;
    }

    Logger::Log("STARTUP", "GetStartupInfoA interceptado!");

    // Restaura o prologo original no momento do disparo.
    DWORD oldProtect = 0;
    if (VirtualProtect(pOriginalGetStartupInfoA, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(pOriginalGetStartupInfoA, StartupHook::g_OriginalGetStartupInfoA, 5);
        VirtualProtect(pOriginalGetStartupInfoA, 5, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), pOriginalGetStartupInfoA, 5);
    }
    InterlockedExchange(&g_HookInstalled, 0);

    Logger::Log("STARTUP", "Hook disparado e auto-removido.");

    // Forca neutralizacao no momento do disparo, caso o DllNotify nao a tenha
    // acionado ainda (PS.dll pode ja estar carregado antes da notificacao).
    Logger::Log("STARTUP", "Chamando PsBypass::Neutralize...");
    bool neutralized = PsBypass::Neutralize();
    Logger::Log("STARTUP", "PsBypass::Neutralize retornou %d", neutralized);

    pOriginalGetStartupInfoA(lpStartupInfo);
}

void StartupHook::Install() {
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    pOriginalGetStartupInfoA = (PGETSTARTUPINFOA)GetProcAddress(hKernel32, "GetStartupInfoA");
    if (!pOriginalGetStartupInfoA) {
        Logger::Log("STARTUP", "FALHA: GetStartupInfoA nao encontrado");
        return;
    }

    BYTE* ptr = (BYTE*)pOriginalGetStartupInfoA;
    if (ptr[0] == 0xE9) {
        Logger::Log("STARTUP", "AVISO: GetStartupInfoA ja estava hookado");
    }

    DWORD oldProtect;
    if (!VirtualProtect(pOriginalGetStartupInfoA, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Logger::Log("STARTUP", "FALHA ao mudar protecao de GetStartupInfoA");
        return;
    }

    memcpy(StartupHook::g_OriginalGetStartupInfoA, pOriginalGetStartupInfoA, 5);

    DWORD relativeAddress = (DWORD)((uintptr_t)HookedGetStartupInfoA - (uintptr_t)pOriginalGetStartupInfoA - 5);
    g_HookBytes[0] = 0xE9;
    *(DWORD*)(g_HookBytes + 1) = relativeAddress;
    memcpy(pOriginalGetStartupInfoA, g_HookBytes, 5);

    VirtualProtect(pOriginalGetStartupInfoA, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), pOriginalGetStartupInfoA, 5);
    InterlockedExchange(&g_HookInstalled, 1);
    Logger::Log("STARTUP", "Hook no GetStartupInfoA instalado");
}

void StartupHook::Uninstall() {
    if (!pOriginalGetStartupInfoA || InterlockedCompareExchange(&g_HookInstalled, 0, 1) != 1) {
        return;
    }

    DWORD oldProtect;
    if (VirtualProtect(pOriginalGetStartupInfoA, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(pOriginalGetStartupInfoA, StartupHook::g_OriginalGetStartupInfoA, 5);
        VirtualProtect(pOriginalGetStartupInfoA, 5, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), pOriginalGetStartupInfoA, 5);
    }

    InterlockedExchange(&g_HookTriggered, 0);
    Logger::Log("STARTUP", "Hook no GetStartupInfoA removido");
}
