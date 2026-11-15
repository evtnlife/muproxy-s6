#pragma once

// ============================================================================
// AsyncScanner — Thread de analise que procura por "Main.free" na memoria
//
// Ao inves de hookar VirtualProtect (arriscado), esta thread monitora
// periodicamente as regioes de memoria alocadas pelo PS.dll e busca a
// string "Main.free" (ANSI e Unicode) nelas. Apenas diagnostico.
// ============================================================================
namespace AsyncScanner {
    void Start();
    void Stop();
}
