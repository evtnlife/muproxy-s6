#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// PacketReassembler — Reassemble de stream TCP em pacotes MU Online
//
// O trafego do jogo chega em pedaços arbitrários (um recv pode trazer meio
// pacote, ou vários). Este módulo mantém um buffer por socket e extrai
// pacotes MU completos (C1/C2/C3/C4), entregando cada um ao chamador.
//
// O módulo NÃO decifra nem interpreta o conteúdo: quem chama decide o que
// fazer com cada pacote (decifrar, logar, alimentar os mapas...).
// ============================================================================

namespace PacketReassembler {

// Callback disparado para cada pacote MU completo extraído do buffer.
// ctx: contexto opaco definido pelo chamador.
typedef void (*PacketCallback)(void* ctx, const uint8_t* data, int len);

// Acumula bytes recebidos no buffer do socket
void Append(SOCKET s, const uint8_t* data, int len);

// Extrai do buffer todos os pacotes MU completos e chama cb para cada um.
// Retorna quantos pacotes foram extraídos.
int Flush(SOCKET s, PacketCallback cb, void* ctx);

// Descarta o buffer do socket (conexão encerrada)
void Remove(SOCKET s);

// Tamanho total do pacote MU a partir do buffer.
// Retorna 0 se o header é inválido ou se o pacote ainda está incompleto.
int ReadMuPacketLength(const uint8_t* buf, int len);

} // namespace PacketReassembler
