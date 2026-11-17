#pragma once
#include <cstdint>

// ============================================================================
// UserObjectStruct — Offsets do ObjectCharacter (MuEmu Object.h)
//
// Endereço: *(main.exe + 0x7D28AC8) — ponteiro indireto (oUserObjectStruct)
// (RVA 0x7D28AC8 → absoluto 0x08128AC8, que contém o ponteiro p/ o UserObject)
// Tamanho: 6012 bytes
//
// Contém: nome, level, stats (STR/AGI/VIT/ENE), life, mana, shield,
// attack speed, damage, magic list, equipment, inventory money.
// DIFERENTE de CharacterStruct (que tem skills na UI).
//
// NOTA: os offsets a partir de OFF_LEVEL foram corrigidos em 2026-11-17
// via x32dbg. O campo em 0x0C é um DWORD (não 2 bytes), então tudo a
// partir de 0x0E desloca +2. Confirmado: level=85 em 0x10 (0x0055).
// ============================================================================

struct UserObjectStruct {
    // ---- Cabeçalho ----
    static constexpr int OFF_NAME       = 0x00;  // char[11] nome
    static constexpr int OFF_CLASS      = 0x0B;  // byte class (0=DW, 1=SM, 2=ELF...)
    static constexpr int OFF_UNK_0C     = 0x0C;  // dword (4 bytes) — era 2 bytes, agora é dword
    static constexpr int OFF_LEVEL      = 0x10;  // word level (85 = 0x55)
    static constexpr int OFF_EXP        = 0x12;  // dword experience
    static constexpr int OFF_NEXTEXP    = 0x16;  // dword next experience

    // ---- Stats base (WORDs) ----
    static constexpr int OFF_STR        = 0x1A;  // word strength
    static constexpr int OFF_AGI        = 0x1C;  // word dexterity
    static constexpr int OFF_VIT        = 0x1E;  // word vitality
    static constexpr int OFF_ENE        = 0x20;  // word energy
    static constexpr int OFF_LEADERSHIP = 0x22;  // word leadership (DL)

    // ---- Vida/Mana/Shield (WORDs) ----
    static constexpr int OFF_LIFE       = 0x24;  // word current life
    static constexpr int OFF_MANA       = 0x26;  // word current mana
    static constexpr int OFF_MAXLIFE    = 0x28;  // word max life
    static constexpr int OFF_MAXMANA    = 0x2A;  // word max mana
    static constexpr int OFF_SHIELD     = 0x2C;  // word current shield (SD)
    static constexpr int OFF_MAXSHIELD  = 0x2E;  // word max shield (SD)
    static constexpr int OFF_ATTACKRATE = 0x30;  // word attack rate (AR)
    static constexpr int OFF_DEFENSERATE= 0x32;  // word defense rate (DR)

    // ---- Stats adicionais (WORDs) ----
    static constexpr int OFF_ADDSTR     = 0x34;  // short add strength
    static constexpr int OFF_ADDAGI     = 0x36;  // short add dexterity
    static constexpr int OFF_ADDVIT     = 0x38;  // short add vitality
    static constexpr int OFF_ADDENE     = 0x3A;  // short add energy
    static constexpr int OFF_ADDLEAD    = 0x40;  // word add leadership
    static constexpr int OFF_STAMINA    = 0x42;  // word stamina
    static constexpr int OFF_MAXSTAMINA = 0x44;  // word max stamina

    // ---- Fruit points ----
    static constexpr int OFF_ADDPOINT   = 0x4E;  // word fruit add point
    static constexpr int OFF_MAXADDPOINT= 0x50;  // word max fruit add point
    static constexpr int OFF_MINUSPOINT = 0x52;  // word fruit sub point
    static constexpr int OFF_MAXMINUS   = 0x54;  // word max fruit sub point

    // ---- Combate ----
    static constexpr int OFF_ATTACKSPEED= 0x56;  // word attack speed
    static constexpr int OFF_DAMAGERATE = 0x58;  // word damage rate
    static constexpr int OFF_DAMAGEMIN  = 0x5A;  // word damage min
    static constexpr int OFF_DAMAGEMAX  = 0x5C;  // word damage max
    static constexpr int OFF_MAGICSPEED = 0x62;  // word magic speed

    // ---- Level points e magias ----
    static constexpr int OFF_LEVELPOINT = 0x76;  // word level up points
    static constexpr int OFF_MAGICCOUNT = 0x78;  // byte magic count
    static constexpr int OFF_MAGICLIST  = 0x7A;  // WORD[650] magic list

    // ---- Equipment (12 slots × 107 bytes) ----
    static constexpr int OFF_EQUIPMENT  = 0x1242; // ObjectItem[12]
    static constexpr int ITEM_SIZE      = 107;    // sizeof(ObjectItem)

    // ---- Money ----
    static constexpr int OFF_MONEY_INV  = 0x1746; // dword money inventory
    static constexpr int OFF_MONEY_WH   = 0x174A; // dword money warehouse
};
