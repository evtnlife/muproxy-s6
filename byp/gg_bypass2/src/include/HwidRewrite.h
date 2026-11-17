#pragma once
#include "sdk.h"

// ============================================================================
// HwidRewrite — reescreve o HWID no pacote de login (C1 F1) antes de enviar.
//
// O servidor usa o HWID (36 chars ASCII "XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX")
// para limitar uma sessão por máquina. Este módulo troca o HWID do pacote de
// login por um valor por-instância, derivado deterministicamente da base
// (MUPROXY_BASE): instância base 0 mantém o HWID real; base > 0 recebe
//   HWID' = HWID com o último DWORD XOR (base * 2654435761u)
// (estável entre execuções da mesma instância, diferente entre instâncias).
//
// Override manual: a variável de ambiente MUPROXY_HWID (formato
// XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX) força um HWID específico para a
// instância, ignorando a derivação.
//
// Fail-safe: qualquer falha (pacote não é login, padrão não encontrado,
// decifra/recifra falhou) => o pacote original é enviado sem alteração.
// ============================================================================

namespace HwidRewrite {

// Lê MUPROXY_HWID (se houver) e loga a configuração. Chamar no attach.
void Initialize();

// Se `buf` for o pacote de login (C3+SM ou C1 direto, opcode F1) contendo o
// HWID, reescreve-o com o HWID desta instância e devolve true com o resultado
// em `out` (mesmo tamanho de `buf` — o HWID tem sempre 36 bytes).
// Caso contrário devolve false e `out` não é tocado (o chamador envia o original).
bool RewriteIfLogin(const char* buf, int len, char* out, int outCap, int* outLen);

} // namespace HwidRewrite
