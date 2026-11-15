#pragma once

#include <cstdint>

// ============================================================================
// MuOpcodes — Códigos de operação dos pacotes do MU Online
//
// Cada pacote começa com um header:
//   C1 XX YY → tamanho 1 byte, tipo C1 (não-encrypted, 1-byte size)
//   C2 XX XX YY → tamanho 2 bytes, tipo C2 (não-encrypted, 2-byte size)
//   C3 XX YY → tamanho 1 byte, tipo C3 (encrypted, 1-byte size)
//   C4 XX XX YY → tamanho 2 bytes, tipo C4 (encrypted, 2-byte size)
//
// O byte após o tamanho (YY acima) é o opcode/subtype.
// ============================================================================

namespace MuOpcodes {

using u8 = uint8_t;

// ---- Client → Server (send) ----
constexpr u8 MOVE                  = 0xD3;  // D3 — movimento do personagem
constexpr u8 ATTACK                 = 0x11;  // 11 — ataque normal a alvo (protocol_code2)
constexpr u8 SKILL_ATTACK           = 0xE5;  // E5 — ataque com skill (C3 encrypted, 13 bytes)
constexpr u8 TARGETED_SKILL         = 0x19;  // 19 — ataque com skill: C1 07 19 [skillH][skillL][indexH][indexL] + XorData + Layer-1
constexpr u8 MAGIC_ATTACK           = 0xD7;  // D7 — ataque mágico (skill, legacy)
constexpr u8 TALK_NPC              = 0x30;  // 30 — falar com NPC
constexpr u8 CREATE_CHARACTER      = 0x31;  // 31 — criar personagem
constexpr u8 DELETE_CHARACTER      = 0x32;  // 32 — deletar personagem
constexpr u8 SELECT_CHARACTER     = 0x33;  // 33 — selecionar personagem
constexpr u8 HIT_REQUEST           = 0x3D;  // 3D — request de dano recebido

// ---- Server → Client (recv) ----
constexpr u8 OBJECT_LIST           = 0x13;  // 13 — lista de objetos no viewport
constexpr u8 OBJECT_ADD            = 0x12;  // 12 — objeto entrou no viewport
constexpr u8 OBJECT_REMOVE         = 0x18;  // 18 — objeto saiu do viewport
constexpr u8 OBJECT_MOVE           = 0x11;  // 11 — objeto moveu
constexpr u8 OBJECT_ATTACK         = 0x14;  // 14 — animação de ataque
constexpr u8 DAMAGE_NUMBER         = 0x15;  // 15 — número de dano flutuante
constexpr u8 HP_UPDATE             = 0x16;  // 16 — update de HP
constexpr u8 MAP_CHANGE            = 0x03;  // 03 — mudança de mapa
constexpr u8 SKILL_LIST            = 0xF3;  // F3 — lista de skills

// ---- Tipos de ataque ----
constexpr u8 ATKTYPE_NORMAL        = 0x00;  // Ataque normal (melee)
constexpr u8 ATKTYPE_SKILL         = 0x01;  // Ataque com skill

// ---- Header types ----
constexpr u8 HEADER_C1 = 0xC1;  // Non-encrypted, 1-byte size
constexpr u8 HEADER_C2 = 0xC2;  // Non-encrypted, 2-byte size
constexpr u8 HEADER_C3 = 0xC3;  // Encrypted, 1-byte size
constexpr u8 HEADER_C4 = 0xC4;  // Encrypted, 2-byte size

// ---- Tamanhos de pacote ----
constexpr u8 SIZE_ATTACK_NORMAL     = 7;   // C1 07 11 <targetHi> <targetLo> <action> <dir>
constexpr u8 SIZE_ATTACK_SKILL     = 13;  // C3 0D E5 <10 bytes payload>

} // namespace MuOpcodes