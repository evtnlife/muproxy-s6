#pragma once
#include "sdk.h"

// ============================================================================
// NameRewriter — Rewrite de nome/classe no pacote de create-char (0x31)
//
// Algoritmo de encoding (descoberto via engenharia reversa no cliente):
//   key = [CF, DB, 23, C5, A3, CA, 33, C1, CC, 66]
//   enc[0] = name[0] ^ key[0]
//   enc[1] = name[1] ^ key[1]
//   enc[i] = name[i] ^ enc[i-1] ^ key[i]  (i >= 2)
//   classByte = class ^ enc[8] ^ 0x63
//
// ATENÇÃO: o rewrite NÃO está ligado a nenhum hook hoje — EncodeName e
// IsCreateCharPacket são as ferramentas de RE do processo de create-char,
// preservadas aqui como módulo independente (antes viviam no PacketSniffer).
// ============================================================================

namespace NameRewriter {

// Chave de encoding do nome (10 bytes)
extern const BYTE g_NameKey[10];

// Nome e classe desejados no create-char
extern const char* g_RewriteName;   // "HexDL"
extern const BYTE  g_RewriteClass;  // 0x10 (Dark Lord, client-side)

// Codifica nome + classe no layout do pacote de create-char.
// outEncoded precisa de 11 bytes (10 do nome + 1 da classe).
void EncodeName(const char* name, BYTE classId, BYTE* outEncoded);

// Detecta o pacote de create-char (C1 0F 31 C3)
bool IsCreateCharPacket(const char* data, int len);

} // namespace NameRewriter
