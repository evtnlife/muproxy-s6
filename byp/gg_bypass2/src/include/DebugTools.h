#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// DebugTools — Ferramentas de diagnóstico/RE (dump, hexdump, scan)
//
// Funções de inspeção da memória do jogo (ObjectManager, slots, scans).
// Separadas do AttackModule (D4) para manter o módulo de ataque enxuto.
//
// São ferramentas interativas de RE: o usuário dispara via comando do
// MuProxy2 (ex.: CMD ATTACK DUMP -> DumpObjectManager).
//
// Comandos registrados (MessageRegistry, formato "TAG|payload"):
//   ATTACK_DUMP|              → despeja o ObjectManager completo
//   ATTACK_HEXDUMP|slot=N     → hex dump do slot N
//   ATTACK_SCANVALUE|value=X  → busca o WORD X em todos os slots
//   ATTACK_SCANSLOT|slot=N|value=X → busca o WORD X em UM slot
//   ATTACK_DIRSCAN|           → procura o byte de direção no player local
//
// O tag de log continua "ATTACK" para manter as linhas de log idênticas
// às de antes da extração.
// ============================================================================

namespace DebugTools {

// ---- Leituras crash-safe da memória do jogo ----
// Retornam valores sentinela em caso de access violation:
//   DWORD → 0xDEADDEAD, WORD → 0xDEAD, BYTE → 0xFF, float → -9999.0f
DWORD SafeReadDword(uintptr_t addr);
WORD  SafeReadWord(uintptr_t addr);
BYTE  SafeReadByte(uintptr_t addr);
float SafeReadFloat(uintptr_t addr);

// DIAGNÓSTICO: despeja o ObjectManager completo com viewport scan
void DumpObjectManager();

// HEX DUMP: despeja o slot inteiro (0x598 bytes) com anotações
void HexDumpSlot(int slotIndex);

// SCANVALUE: busca um valor WORD em todos os slots do ObjectManager
void ScanForValue(uint16_t searchValue);

// SCANSLOT: busca um valor WORD em UM slot específico (hex dump + busca)
void ScanSlotForValue(int slotIndex, uint16_t searchValue);

// DIRSCAN: escaneia o player local buscando o byte de direção
void DirScan();

} // namespace DebugTools
