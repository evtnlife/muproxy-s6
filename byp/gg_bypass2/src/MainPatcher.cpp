#include "include/MainPatcher.h"
#include "include/Config.h"
#include "include/Logger.h"
#include "include/PsBypass.h"
#include "include/PsPacketHook.h"
#include "include/PacketSniffer.h"
#include "include/sdk.h"

static HANDLE g_hThread = NULL;
static volatile LONG g_Running = 0;

static DWORD WINAPI MainPatcherThread(LPVOID) {
    Logger::Log("MAINPATCHER", "Thread de patch iniciada; aguardando Main.dll...");

    HMODULE hMain = NULL;
    for (int i = 0; i < 200 && !hMain && g_Running; i++) {
        hMain = GetModuleHandleA("Main.dll");
        if (!hMain) Sleep(100);
    }

    if (!hMain) {
        Logger::Log("MAINPATCHER", "Main.dll nao apareceu apos 20s; patches nao aplicados");
        return 1;
    }

    Logger::Log("MAINPATCHER", "Main.dll carregado em 0x%p; aplicando patches...", hMain);

    bool ok = true;
    if (Config::FIX_MAIN_FREE_LANGUAGE) {
        ok &= PsBypass::FixMainFreeLanguage();
    }
    if (Config::PATCH_MAIN_FREE_LANGUAGE_JUMP) {
        ok &= PsBypass::PatchMainFreeLanguageJump();
    }
    if (Config::PATCH_MAIN_FREE_JUMP) {
        ok &= PsBypass::PatchMainFreeJump();
    }
    if (Config::PATCH_PS_PACKET_CALLER) {
        ok &= PsBypass::PatchPsPacketCaller();
    }
    if (Config::HOOK_PS_PACKET_DISPATCHER) {
        PsPacketHook::Install();
    }
    if (Config::PACKET_SNIFFER_ENABLED) {
        PacketSniffer::Install();
    }

    Logger::Log("MAINPATCHER", "Patches aplicados (ok=%d)", ok);
    return 0;
}

void MainPatcher::Start() {
    if (InterlockedCompareExchange(&g_Running, 1, 0) != 0) return;

    g_hThread = CreateThread(NULL, 0, MainPatcherThread, NULL, 0, NULL);
    if (!g_hThread) {
        InterlockedExchange(&g_Running, 0);
        Logger::Log("MAINPATCHER", "FALHA ao criar thread de patch");
    }
}

void MainPatcher::Stop() {
    InterlockedExchange(&g_Running, 0);
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 2000);
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
}
