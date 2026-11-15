#pragma once
#include "sdk.h"

// ============================================================================
// PsBypass — Neutralizador do módulo de proteção PS.dll
//
// O PS.dll é um protector anti-tamper/anti-debug customizado do servidor
// BlackRock MU. Ele se disfarça de módulo de áudio (wzAudio*) e instala
// hooks via GetStartupInfoA para monitorar o processo.
//
// Esta classe detecta o PS.dll carregado e o neutraliza de forma segura,
// preservando as exports wzAudio* para que o jogo continue funcionando.
// ============================================================================
class PsBypass {
public:
    // Verifica se PS.dll está carregado e aplica neutralização
    static bool Neutralize();

    // Retorna a base do PS.dll na memória (0 se não carregado)
    static HMODULE GetPsModule();

    // Retorna TRUE se o módulo parece ser o protector (pelas exports)
    static bool IsPsProtector(HMODULE hMod);

    // Preenche o buffer de idioma Main.free quando o PS.dll foi removido
    static bool FixMainFreeLanguage();

    // Patch no salto condicional de Main.dll para sempre ignorar o erro
    static bool PatchMainFreeJump();

    // Patch no salto da validação de idioma do loading (Main.dll+0x24C91)
    static bool PatchMainFreeLanguageJump();

    // Patch no caller do dispatcher de pacotes PS (main.exe+0x25FD83)
    static bool PatchPsPacketCaller();

private:
    // Restaura GetStartupInfoA para o original (remove hook do PS.dll)
    static bool RestoreGetStartupInfoA();

    // Suspende threads cujo start address esteja dentro do PS.dll
    static bool SuspendPsThreads(HMODULE hMod);
};
