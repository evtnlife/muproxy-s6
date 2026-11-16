#pragma once
#include "sdk.h"
#include <cstdint>
#include "GameAddresses.h"

// ============================================================================
// GameMemory::Globals — Bases dos módulos e valores globais do main.exe
//
// Endereços base são relativos ao módulo main.exe e resolvidos em runtime.
// ============================================================================

namespace GameMemory {
namespace Globals {

// Resolve base do main.exe (cache interno)
uintptr_t MainBase();

// Resolve base do Main.dll (cache interno)
uintptr_t MainDllBase();

// Número do mapa atual
inline DWORD MapNumber() {
    uintptr_t base = MainBase();
    return base ? *(DWORD*)(base + GameAddresses::MAP_NUMBER) : 0;
}

// Índice de entidade do alvo atual (global 0x081C03A4).
// O jogo grava o alvo aqui quando o jogador clica num monstro — é o que o
// jogo coloca direto no pacote de skill attack (sem XOR).
// NOTE: 0x081C03A4 é um endereço ABSOLUTO em main.exe, não um RVA.
inline uint16_t TargetEntityIndex() {
    uintptr_t addr = GameAddresses::TARGET_ENTITY_INDEX_GLOBAL;
    return addr ? *(uint16_t*)addr : 0;
}

// Índice de slot do alvo atual (global 0x081C03A0). -1 = nenhum alvo.
inline int32_t TargetSlotIndex() {
    uintptr_t addr = GameAddresses::TARGET_SLOT_GLOBAL;
    return addr ? *(int32_t*)addr : -1;
}

} // namespace Globals
} // namespace GameMemory
