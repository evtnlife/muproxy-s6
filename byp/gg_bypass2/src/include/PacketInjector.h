#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// PacketInjector — Porta única de injeção de pacotes no game server
//
// O XorData (Camada 2) é aplicado UMA vez, num único lugar, para todos os
// opcodes. O pipeline final é escolhido pelo opcode (byte 2 do plaintext):
//
//   0x11 (ataque normal) → Layer-1 → wire (C1 puro)
//   0x19 (skill attack)  → SM(serial + payload) → C3 0D [11] → Layer-1 → wire
//
// Exemplo (0x11): C1 07 11 00 1A 78 04
//   → XorData  : C1 07 11 EF ED B6 77
//   → Layer-1  : FE 38 2E C0 C2 8B 48 (wire)
//
// Exemplo (0x19): C1 07 19 00 3C 02 77
//   → XorData  : C1 07 19 E7 C3 E2 50
//   → SM       : 11 bytes (serial 0x08793700 + payload)
//   → C3 0D [11] → Layer-1 → wire
//
// O transporte (socket do game server + send original) pertence ao módulo de
// hooks: ele registra o callback de envio via SetSender().
// ============================================================================

namespace PacketInjector {

// Callback de envio no socket do game server.
// Recebe os bytes JÁ CIFRADOS (prontos para o wire).
typedef bool (*SendFn)(const uint8_t* data, int len);

// Registra o transporte usado pela injeção (chamado pelo módulo de hooks).
void SetSender(SendFn fn);

// Porta ÚNICA de injeção: recebe o plaintext real do pacote e escolhe o
// pipeline pelo opcode (0x11 normal, 0x19 skill).
bool Inject(const uint8_t* plaintext, int len);

// Injeta dados que JÁ estão na forma XorData (comando SEND_PACKET / "SEND"):
// aplica apenas a Layer-1 (XOR+sub) e envia.
bool InjectXorData(const uint8_t* data, int len);

} // namespace PacketInjector
