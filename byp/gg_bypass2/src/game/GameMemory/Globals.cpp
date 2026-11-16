// game/GameMemory/Globals.cpp — Bases dos módulos e valores globais do main.exe
#include "GameMemory/Globals.h"
#include "Logger.h"

namespace GameMemory {
namespace Globals {

static uintptr_t g_MainBase = 0;
static uintptr_t g_MainDllBase = 0;

uintptr_t MainBase() {
    if (g_MainBase == 0) {
        // Tenta "main.exe" primeiro
        g_MainBase = (uintptr_t)GetModuleHandleA("main.exe");
        // Fallback: GetModuleHandle(NULL) = base do executavel principal
        if (!g_MainBase) {
            g_MainBase = (uintptr_t)GetModuleHandleA(NULL);
        }
        if (g_MainBase) {
            Logger::Log("GAMEMEMORY", "main.exe base = 0x%p", (void*)g_MainBase);
        }
    }
    return g_MainBase;
}

uintptr_t MainDllBase() {
    if (g_MainDllBase == 0) {
        g_MainDllBase = (uintptr_t)GetModuleHandleA("Main.dll");
        if (g_MainDllBase) {
            Logger::Log("GAMEMEMORY", "Main.dll base = 0x%p", (void*)g_MainDllBase);
        }
    }
    return g_MainDllBase;
}

} // namespace Globals
} // namespace GameMemory
