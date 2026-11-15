#pragma once
#include "sdk.h"

// ============================================================================
// PsPacketHook — Hook no dispatcher de pacotes GameGuard/PS em Main.dll
//
// Sem o PS.dll ativo, Main.dll!PsPacketHandler (RVA 0x13F510) falha nas
// validações de pacotes do servidor e dispara MessageBoxA de erro.
// Esta classe instala um hook no início dessa função para forçar retorno true.
// ============================================================================
class PsPacketHook {
public:
    // Instala hook assim que Main.dll estiver carregado.
    static bool Install();

    // Remove hook (restaura bytes originais).
    static void Uninstall();

private:
    static bool InstallInlineHook(void* target, void* hook);
    static bool RestoreInlineHook(void* target);

    static BYTE* g_Target;
    static BYTE g_OriginalBytes[5];
    static bool g_Installed;
};
