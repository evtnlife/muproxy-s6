#pragma once
#include <cstdint>

// ============================================================================
// SkillMap — Mapa dinâmico SkillNumber -> SkillIndex
//
// O servidor BlackRock MU NÃO envia o pacote OpenMU C1 F3 11 (SkillListUpdate).
// Em vez disso, envia C2 F3 E2 (SkillListMulti) com skills de entidades do
// viewport. Cada entrada tem 24 bytes:
//   [0..1]   SkillNumber (2 bytes, little-endian)
//   [2..3]   0x0D02 (constante)
//   [4..7]   float (pos X)
//   [8..11]  0
//   [12..15] float (pos Y)
//   [16..19] 0
//   [20..21] SkillLevel (2 bytes, little-endian)
//   [22..23] 0
//
// O pacote de ataque 0x19 (TargetedSkill) usa SkillIndex (1 byte), NÃO o
// SkillNumber (2 bytes). OpenMU: "SkillIndex = index of the skill in the list".
//
// Pacote real do jogo: C1 07 19 E7 EE CF 12 → skill byte = 0xE7.
// Inferno = 0x01E7 → SkillIndex = 0xE7 = skillNumber & 0xFF (byte baixo).
//
// Este mapa converte SkillNumber (2 bytes) -> SkillIndex (1 byte).
// ============================================================================

namespace SkillMap {

// Chamado quando um pacote de skill (F3 11 ou F3 E2) é recebido (já decifrado).
// data aponta para o pacote completo (header + F3 + subcode + payload).
void OnSkillListPacket(const uint8_t* data, int len);

// Retorna o SkillIndex (1 byte) para um SkillNumber (2 bytes).
// Retorna 0 se não encontrado.
uint8_t GetSkillIndex(uint16_t skillNumber);

// Retorna o SkillNumber (2 bytes) para um SkillIndex (1 byte).
// Retorna 0 se não encontrado.
uint16_t GetSkillNumber(uint8_t skillIndex);

// Limpa o mapa.
void Clear();

} // namespace SkillMap
