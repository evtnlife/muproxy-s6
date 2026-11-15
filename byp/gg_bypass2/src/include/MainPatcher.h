#pragma once
#include "sdk.h"

// ============================================================================
// MainPatcher — Aplica patches em Main.dll de forma assíncrona
//
// O Main.dll é carregado lazy pelo main.exe. O StartupHook dispara muito
// cedo (antes do Main.dll existir), então PsBypass::Neutralize() falha
// ao tentar aplicar patches. Esta thread espera Main.dll aparecer e então
// aplica os patches de Main.free e idioma na hora certa.
// ============================================================================
namespace MainPatcher {
    // Inicia thread que monitora o carregamento de Main.dll e aplica patches.
    void Start();

    // Sinaliza para a thread encerrar e aguarda.
    void Stop();
}
