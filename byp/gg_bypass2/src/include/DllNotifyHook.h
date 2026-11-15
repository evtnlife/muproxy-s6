#pragma once
#include "sdk.h"

// Registra notificacao de carga de DLL via LdrRegisterDllNotification.
// Quando PS.dll for carregado, chama PsBypass::Neutralize() se ativo.
namespace DllNotifyHook {
    bool Install();
    void Uninstall();
}
