#pragma once
#include "sdk.h"

// ============================================================================
// StartupHook — Hook simples em GetStartupInfoA
//
// Instala/restaura um hook no prologo de kernel32!GetStartupInfoA.
// Quando ativo, notifica no log que foi disparado. Pode ser desinstalado
// via Uninstall() para análise sem interferência.
//
// Os 5 bytes originais de GetStartupInfoA sao salvos aqui para que
// PsBypass possa restaura-los de forma fiel mesmo se o protector
// usar um hook diferente de JMP rel32.
// ============================================================================
namespace StartupHook {
    // Bytes originais do prologo de kernel32!GetStartupInfoA (5 bytes)
    extern BYTE g_OriginalGetStartupInfoA[5];

    void Install();
    void Uninstall();
}
