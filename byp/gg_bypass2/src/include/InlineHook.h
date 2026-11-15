#pragma once
#include "sdk.h"

// ============================================================================
// InlineHook — Hook inline genérico e reutilizável para funções x86
//
// Instala um JMP rel32 (5 bytes) ou mais no target, redirecionando para a
// função de hook. Cria um trampoline que executa os bytes originais antes de
// pular de volta para (target+stolenLen).
//
// IMPORTANTE: O tamanho de bytes roubados (stolenLen) deve cobrir instruções
// completas. Se o prólogo da função tem instruções que não se alinham em 5
// bytes, stolenLen deve ser maior para não cortar instruções no meio.
//
// Exemplo: push ebp (1) + mov ebp,esp (2) + push ecx (1) + mov [ebp-4],ecx (3)
//          = 7 bytes. Se stolenLen=5, corta "mov [ebp-4],ecx" no meio!
//          Use stolenLen=7 para copiar instruções completas.
//
// Uso:
//   static InlineHook g_Hook;
//   g_Hook.Install(pTarget, pMyHook, 7);  // roubar 7 bytes
//   ... chamar g_Hook.CallOrig() para executar a função original ...
//   g_Hook.Uninstall();
// ============================================================================

class InlineHook {
public:
    InlineHook() = default;
    ~InlineHook() { Uninstall(); }

    // Instala hook: target -> hookFunc
    // stolenLen: quantos bytes copiar para o trampoline (padrão 5, mínimo 5)
    // Deve cobrir instruções completas — se 5 corta uma instrução, use 7+
    bool Install(void* target, void* hookFunc, int stolenLen = 5);

    // Restaura bytes originais e libera trampoline
    void Uninstall();

    // Retorna o endereço do trampoline (função original com prologue restaurado)
    void* GetTrampoline() const { return m_trampoline; }

    bool IsInstalled() const { return m_installed; }
    void* GetTarget() const { return m_target; }

private:
    void*  m_target    = nullptr;
    void*  m_trampoline = nullptr;
    int    m_stolenLen = 5;
    BYTE   m_original[16] = {0};  // máximo 16 bytes roubados
    bool   m_installed = false;

    // Nao copiar
    InlineHook(const InlineHook&) = delete;
    InlineHook& operator=(const InlineHook&) = delete;
};