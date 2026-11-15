#include "include/PsPacketHook.h"
#include "include/Config.h"
#include "include/Logger.h"
#include "include/sdk.h"

BYTE* PsPacketHook::g_Target = nullptr;
BYTE PsPacketHook::g_OriginalBytes[5] = {0};
bool PsPacketHook::g_Installed = false;

// Stub de trampoline: salta para o hook C++.
// Layout: mov eax, hook_addr ; jmp eax
// Bytes: B8 xx xx xx xx ; FF E0
#pragma pack(push, 1)
struct TrampolineStub {
    BYTE mov_eax;      // 0xB8
    DWORD hook_addr;   // endereço do nosso hook
    BYTE jmp_eax[2];   // 0xFF 0xE0
};
#pragma pack(pop)

static TrampolineStub g_Stub;

// Hook que força retorno true no dispatcher.
// A função original tem calling convention __thiscall/stdcall com retorno em EAX.
// Nós simplesmente retornamos 1.
extern "C" __declspec(naked) void PsPacketHook_Entry() {
    __asm {
        mov eax, 1
        retn
    }
}

bool PsPacketHook::InstallInlineHook(void* target, void* hook) {
    if (!target || !hook) return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Logger::Log("PSPACKETHOOK", "FALHA ao alterar protecao do target");
        return false;
    }

    BYTE* t = (BYTE*)target;
    memcpy(g_OriginalBytes, t, 5);

    // Prepara stub em memória alocada perto do target ou usa um buffer estático executável.
    // Simplificação: escrevemos o stub em uma variável global marcada como executável.
    // O stub contém: mov eax, hook ; jmp eax (7 bytes).
    DWORD stubOldProtect = 0;
    if (!VirtualProtect(&g_Stub, sizeof(g_Stub), PAGE_EXECUTE_READWRITE, &stubOldProtect)) {
        VirtualProtect(target, 5, oldProtect, &oldProtect);
        return false;
    }
    g_Stub.mov_eax = 0xB8;
    g_Stub.hook_addr = (DWORD)hook;
    g_Stub.jmp_eax[0] = 0xFF;
    g_Stub.jmp_eax[1] = 0xE0;
    VirtualProtect(&g_Stub, sizeof(g_Stub), PAGE_EXECUTE_READ, &stubOldProtect);
    FlushInstructionCache(GetCurrentProcess(), &g_Stub, sizeof(g_Stub));

    // Patch no target: JMP rel32 para o stub.
    LONG rel = (LONG)((BYTE*)&g_Stub - (t + 5));
    t[0] = 0xE9;
    *(LONG*)(t + 1) = rel;

    VirtualProtect(target, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    return true;
}

bool PsPacketHook::RestoreInlineHook(void* target) {
    if (!target) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    memcpy(target, g_OriginalBytes, 5);
    VirtualProtect(target, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    return true;
}

bool PsPacketHook::Install() {
    if (g_Installed) return true;

    HMODULE hMain = nullptr;
    for (int i = 0; i < 100 && !hMain; i++) {
        hMain = GetModuleHandleA("Main.dll");
        if (!hMain) Sleep(50);
    }
    if (!hMain) {
        Logger::Log("PSPACKETHOOK", "Main.dll nao carregado apos timeout; hook do dispatcher nao instalado");
        return false;
    }

    g_Target = (BYTE*)((uintptr_t)hMain + Config::PS_PACKET_DISPATCHER_RVA);
    Logger::Log("PSPACKETHOOK", "Instalando hook no dispatcher PS em 0x%p bytes=%02X %02X %02X %02X %02X", g_Target, g_Target[0], g_Target[1], g_Target[2], g_Target[3], g_Target[4]);

    if (!InstallInlineHook(g_Target, (void*)PsPacketHook_Entry)) {
        Logger::Log("PSPACKETHOOK", "FALHA ao instalar hook");
        return false;
    }

    g_Installed = true;
    Logger::Log("PSPACKETHOOK", "Hook instalado com sucesso");
    return true;
}

void PsPacketHook::Uninstall() {
    if (!g_Installed || !g_Target) return;
    RestoreInlineHook(g_Target);
    g_Installed = false;
    g_Target = nullptr;
    Logger::Log("PSPACKETHOOK", "Hook removido");
}
