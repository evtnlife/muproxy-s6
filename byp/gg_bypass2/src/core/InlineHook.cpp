// InlineHook.cpp — Implementação de hook inline genérico para x86
// Suporta stolenLen configurável para não cortar instruções no meio.
#include "InlineHook.h"
#include "Logger.h"

bool InlineHook::Install(void* target, void* hookFunc, int stolenLen) {
    if (!target || !hookFunc) return false;
    if (m_installed) return true;
    if (stolenLen < 5) stolenLen = 5;  // mínimo para JMP rel32
    if (stolenLen > 16) stolenLen = 16; // máximo buffer

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, stolenLen, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Logger::Log("INLINEHOOK", "VirtualProtect falhou em %p", target);
        return false;
    }

    BYTE* t = (BYTE*)target;
    memcpy(m_original, t, stolenLen);
    m_stolenLen = stolenLen;

    // Aloca trampoline com bytes roubados + JMP rel32 (5 bytes)
    int trampSize = stolenLen + 5;  // stolenBytes + JMP rel32
    BYTE* tramp = (BYTE*)VirtualAlloc(nullptr, trampSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) {
        VirtualProtect(target, stolenLen, oldProtect, &oldProtect);
        Logger::Log("INLINEHOOK", "Falha ao alocar trampoline");
        return false;
    }

    // Monta trampoline: bytes originais roubados + JMP de volta para target+stolenLen
    memcpy(tramp, m_original, stolenLen);
    tramp[stolenLen] = 0xE9;  // JMP rel32
    DWORD jmpRel = (DWORD)((BYTE*)target + stolenLen - (tramp + stolenLen + 5));
    memcpy(tramp + stolenLen + 1, &jmpRel, 4);

    // Instala JMP rel32 no target -> hookFunc
    // Preenche com NOPs quaisquer bytes além dos 5 do JMP
    LONG rel = (LONG)((BYTE*)hookFunc - (t + 5));
    t[0] = 0xE9;
    memcpy(t + 1, &rel, 4);
    // NOP out any remaining stolen bytes beyond the 5-byte JMP
    for (int i = 5; i < stolenLen; i++) {
        t[i] = 0x90;  // NOP
    }

    VirtualProtect(target, stolenLen, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, stolenLen);
    FlushInstructionCache(GetCurrentProcess(), tramp, trampSize);

    m_target = target;
    m_trampoline = tramp;
    m_installed = true;

    Logger::Log("INLINEHOOK", "Hook instalado: target=%p hook=%p stolenLen=%d trampoline=%p",
                target, hookFunc, stolenLen, tramp);
    return true;
}

void InlineHook::Uninstall() {
    if (!m_installed || !m_target) return;

    // Restaura bytes originais
    DWORD oldProtect = 0;
    if (VirtualProtect(m_target, m_stolenLen, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(m_target, m_original, m_stolenLen);
        VirtualProtect(m_target, m_stolenLen, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), m_target, m_stolenLen);
    }

    // Libera trampoline
    if (m_trampoline) {
        VirtualFree(m_trampoline, 0, MEM_RELEASE);
        m_trampoline = nullptr;
    }

    m_installed = false;
    m_target = nullptr;
}