#pragma once
#include <cstdint>

// ============================================================================
// CharacterStruct — Offsets da estrutura do personagem no main.exe
//
// Endereço base: main.exe + 0xA96B0C (GameAddresses::CHARACTER_PTR)
// Descoberto via x32dbg memory search pelo nome do personagem.
//
// ATENÇÃO: Esta struct NÃO contém level/stats (STR/AGI/VIT/ENE).
// Level e stats estão em UserObjectStruct (GameAddresses::USER_OBJECT_PTR).
//
// Esta struct contém: nome, classe, e 4 entradas de skills
// com 4 slots cada (total 16 skills mapeadas na UI).
// ============================================================================

struct CharacterStruct {
    // ---- Cabeçalho ----
    static constexpr int OFF_PTR0       = 0x00;  // dword ponteiro (vtable/next)
    static constexpr int OFF_NAME       = 0x0C;  // char[10] nome do personagem
    static constexpr int OFF_STATPOINTS = 0x1A;  // word pontos a distribuir (10 observado)
    static constexpr int OFF_UNK_1C     = 0x1C;  // word (15 observado)
    static constexpr int OFF_PTR1       = 0x30;  // dword ponteiro secundário

    // ---- Atributos ----
    static constexpr int OFF_CLASS      = 0x40;  // word class (2=Dark Wizard)
    static constexpr int OFF_UNK_42     = 0x42;  // word (3 observado)
    static constexpr int OFF_UNK_44     = 0x44;  // word (235 observado)
    static constexpr int OFF_EQUIP      = 0x46;  // 14 bytes equipamento codificado

    // ---- Skills (4 entradas de 24 bytes cada) ----
    // Cada entrada: 16 bytes flags/cooldowns + 4 WORDs de skill IDs
    // Entrada 1 (slots 1-4): +0x68 flags, +0x78 IDs
    // Entrada 2 (slots 5-8): +0x80 flags, +0x90 IDs
    // Entrada 3 (slots 9-12): +0x98 flags, +0xA8 IDs
    // Entrada 4 (slots 13-16): +0xB0 flags, +0xC0 IDs
    static constexpr int OFF_SKILL1     = 0x68;  // 24 bytes: skill entry 1
    static constexpr int OFF_SKILL1_IDS = 0x78;  // 4 WORDs: skill IDs nos slots 1-4
    static constexpr int OFF_SKILL2     = 0x80;  // 24 bytes: skill entry 2
    static constexpr int OFF_SKILL2_IDS = 0x90;  // 4 WORDs: skill IDs nos slots 5-8
    static constexpr int OFF_SKILL3     = 0x98;  // 24 bytes: skill entry 3
    static constexpr int OFF_SKILL3_IDS = 0xA8;  // 4 WORDs: skill IDs nos slots 9-12
    static constexpr int OFF_SKILL4     = 0xB0;  // 24 bytes: skill entry 4
    static constexpr int OFF_SKILL4_IDS = 0xC0;  // 4 WORDs: skill IDs nos slots 13-16

    // ---- Constantes ----
    static constexpr int SKILL_ENTRY_SIZE = 24;   // bytes por entrada de skill
    static constexpr int MAX_SKILL_ENTRIES = 4;   // total de entradas
    static constexpr int MAX_SKILL_SLOTS   = 16;  // total de slots (4 entradas x 4)
};
