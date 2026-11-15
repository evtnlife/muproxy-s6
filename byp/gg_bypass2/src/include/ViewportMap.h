#pragma once
#include <cstdint>

// ============================================================================
// ViewportMap — Mapa dinâmico viewport index -> attack index
//
// O servidor MU usa DOIS índices diferentes para uma mesma entidade:
//   1. Viewport index  — usado nos pacotes 0x13/0x14/D4 (spawn/movimento).
//   2. Attack index    — usado nos pacotes de ataque 0x11/0x19.
//
// A fórmula do attack index (documentada em AttackMonsterPacket.cs):
//   attackIndex = (viewport_idxLo << 8) | monsterClass_lo
//
// Onde viewport_idxLo é o byte baixo do índice de viewport (sem flags) e
// monsterClass_lo é o byte baixo da classe do monstro (sem flags).
//
// O pacote 0x13 (AddNpcsToScope) traz, para cada monstro:
//   index[2] (viewport, com flags), type[2] (class, com flags), x, y (tiles).
//
// CORRELAÇÃO PRECISA (por índice, NÃO por posição):
//   - OFF_INDEX (0x7E) do ObjectManager == viewport index (mesma sequência).
//   - OFF_ID (0x84) do ObjectManager == monster class.
// Mapeamos viewportIndex (2 bytes) -> attackIndex (2 bytes).
// ============================================================================

namespace ViewportMap {

// Chamado quando um pacote 0x13 (AddNpcsToScope) é recebido (já decifrado).
// data aponta para o pacote completo (header C2 + opcode 0x13 + payload).
void OnViewportPacket(const uint8_t* data, int len);

// Retorna o attack index para um viewport index (== OFF_INDEX do ObjectManager).
// Retorna 0 se não encontrado.
uint16_t GetAttackIndex(uint16_t viewportIndex);

// Limpa o mapa (ex: troca de mapa).
void Clear();

} // namespace ViewportMap
