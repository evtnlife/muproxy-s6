#pragma once
#include "sdk.h"

// ============================================================================
// C3Decoder — Decifra e loga o pacote C1/C2 interno de um C3/C4
//
// O NativeDecrypt do jogo falha em C2S (o DecKey do Obj1 em 0x8797778 é zero),
// então usamos o SimpleModulusCipher com a chave decifrada.
//
// É uma ferramenta de RE: o resultado aparece no log ("SEND C3 decoded: [...]").
// ============================================================================

namespace C3Decoder {

// Loga o C1/C2 interno de um pacote C3/C4 já decifrado (Layer-1).
// displayBuf: o pacote com o header C3/C4; displayLen: tamanho.
// Não faz nada se o pacote não for C3/C4.
void LogDecoded(const char* displayBuf, int displayLen);

} // namespace C3Decoder
