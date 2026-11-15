#pragma once
#include <cstdint>

// ============================================================================
// UserObjectStruct — Offsets do ObjectCharacter (MuEmu Object.h)
//
// Endereço: *(main.exe + 0x8128AC8) — ponteiro indireto (oUserObjectStruct)
// Tamanho: 6012 bytes
//
// Contém: nome, level, stats (STR/AGI/VIT/ENE), life, mana, shield,
// attack speed, damage, magic list, equipment, inventory money.
// DIFERENTE de CharacterStruct (que tem skills na UI).
// ============================================================================

struct UserObjectStruct {
    // ---- Cabeçalho ----
    static constexpr int OFF_NAME       = 0x00;  // char[11] nome
    static constexpr int OFF_CLASS      = 0x0B;  // byte class (0=DW, 1=SM, 2=ELF...)
    static constexpr int OFF_UNK_0C     = 0x0C;  // byte unknown
    static constexpr int OFF_UNK_0D     = 0x0D;  // byte unknown
    static constexpr int OFF_LEVEL      = 0x0E;  // word level (82 = 0x52)
    static constexpr int OFF_EXP        = 0x10;  // dword experience
    static constexpr int OFF_NEXTEXP    = 0x14;  // dword next experience

    // ---- Stats base (WORDs) ----
    static constexpr int OFF_STR        = 0x18;  // word strength
    static constexpr int OFF_AGI        = 0x1A;  // word dexterity
    static constexpr int OFF_VIT        = 0x1C;  // word vitality
    static constexpr int OFF_ENE        = 0x1E;  // word energy
    static constexpr int OFF_LEADERSHIP = 0x20;  // word leadership (DL)

    // ---- Vida/Mana/Shield (WORDs) ----
    static constexpr int OFF_LIFE       = 0x22;  // word current life
    static constexpr int OFF_MANA       = 0x24;  // word current mana
    static constexpr int OFF_MAXLIFE    = 0x26;  // word max life
    static constexpr int OFF_MAXMANA    = 0x28;  // word max mana
    static constexpr int OFF_SHIELD     = 0x2A;  // word current shield (SD)
    static constexpr int OFF_MAXSHIELD  = 0x2C;  // word max shield (SD)
    static constexpr int OFF_ATTACKRATE = 0x2E;  // word attack rate (AR)
    static constexpr int OFF_DEFENSERATE= 0x30;  // word defense rate (DR)

    // ---- Stats adicionais (WORDs) ----
    static constexpr int OFF_ADDSTR     = 0x32;  // short add strength
    static constexpr int OFF_ADDAGI     = 0x34;  // short add dexterity
    static constexpr int OFF_ADDVIT     = 0x36;  // short add vitality
    static constexpr int OFF_ADDENE     = 0x38;  // short add energy
    static constexpr int OFF_ADDLEAD    = 0x3E;  // word add leadership
    static constexpr int OFF_STAMINA    = 0x40;  // word stamina
    static constexpr int OFF_MAXSTAMINA = 0x42;  // word max stamina

    // ---- Fruit points ----
    static constexpr int OFF_ADDPOINT   = 0x4C;  // word fruit add point
    static constexpr int OFF_MAXADDPOINT= 0x4E;  // word max fruit add point
    static constexpr int OFF_MINUSPOINT = 0x50;  // word fruit sub point
    static constexpr int OFF_MAXMINUS   = 0x52;  // word max fruit sub point

    // ---- Combate ----
    static constexpr int OFF_ATTACKSPEED= 0x54;  // word attack speed
    static constexpr int OFF_DAMAGERATE = 0x56;  // word damage rate
    static constexpr int OFF_DAMAGEMIN  = 0x58;  // word damage min
    static constexpr int OFF_DAMAGEMAX  = 0x5A;  // word damage max
    static constexpr int OFF_MAGICSPEED = 0x60;  // word magic speed

    // ---- Level points e magias ----
    static constexpr int OFF_LEVELPOINT = 0x74;  // word level up points
    static constexpr int OFF_MAGICCOUNT = 0x76;  // byte magic count
    static constexpr int OFF_MAGICLIST  = 0x78;  // WORD[650] magic list

    // ---- Equipment (12 slots × 107 bytes) ----
    static constexpr int OFF_EQUIPMENT  = 0x1240; // ObjectItem[12]
    static constexpr int ITEM_SIZE      = 107;    // sizeof(ObjectItem)

    // ---- Money ----
    static constexpr int OFF_MONEY_INV  = 0x1744; // dword money inventory
    static constexpr int OFF_MONEY_WH   = 0x1748; // dword money warehouse
};