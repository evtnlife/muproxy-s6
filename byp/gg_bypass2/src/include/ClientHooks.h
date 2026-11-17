#pragma once
#include "sdk.h"

// ============================================================================
// ClientHooks — "vigia" o carregamento do Main.dll pelo cliente.
//
// O Main.dll é carregado dinamicamente (LoadLibrary) em algum momento depois
// que o main.exe sobe. Todos os patches que precisam do Main.dll pronto
// (hook do gerador de HWID, neutralização da checagem de instância única)
// são aplicados AQUI, no instante exato em que o módulo entra.
//
// Sem esse gatilho, a "corrida" entre o cliente gerar o HWID e a gente
// instalar o hook é perdida: o cliente faz auto-login ~1s depois de abrir.
// ============================================================================

namespace ClientHooks {

// Instala o hook de LoadLibraryA (idempotente).
void Initialize();

} // namespace ClientHooks
