#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// C3Decoder — Decifra e loga o pacote C1/C2 interno de um C3/C4
//
// O NativeDecrypt do jogo falha em C2S (o DecKey do Obj1 em 0x8797778 é zero),
// então usamos o SimpleModulusCipher com a chave decifrada.
//
// É uma ferramenta de RE: o resultado aparece no log ("SEND C3 decoded: [...]").
// ============================================================================

namespace C3Decoder {

// Notificado sempre que um C3/C4 é decodificado com sucesso.
// Recebe o pacote INTERNO (header C1/C2 + payload) já em claro (o payload
// ainda está na forma Camada-2 — quem precisar do plaintext aplica o
// UnXorData). É por aqui que o auto-potion enxerga o uso de item do jogador:
// o pacote vem dentro de um C3+SimpleModulus, que só é decodificado aqui.
typedef void (*InnerPacketObserver)(const uint8_t* innerPkt, int innerLen);

void SetInnerPacketObserver(InnerPacketObserver obs);

// Loga o C1/C2 interno de um pacote C3/C4 já decifrado (Layer-1).
// displayBuf: o pacote com o header C3/C4; displayLen: tamanho.
// Não faz nada se o pacote não for C3/C4.
void LogDecoded(const char* displayBuf, int displayLen);

} // namespace C3Decoder
