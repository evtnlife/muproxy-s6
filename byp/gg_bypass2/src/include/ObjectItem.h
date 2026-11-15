#pragma once
#include <cstdint>

// ============================================================================
// ObjectItem — Offsets de um item no inventário (MuEmu Object.h, 107 bytes)
//
// Usado em: UserObjectStruct::OFF_EQUIPMENT (12 slots × 107 bytes)
// ============================================================================

struct ObjectItem {
    static constexpr int OFF_ITEMID       = 0x00;  // short ItemID
    static constexpr int OFF_LEVEL        = 0x02;  // int Level
    static constexpr int OFF_DAMAGEMIN    = 0x09;  // short DamageMin
    static constexpr int OFF_DAMAGEMAX    = 0x0B;  // short DamageMax
    static constexpr int OFF_DURABILITY   = 0x16;  // char Durability
    static constexpr int OFF_EXCELLENT    = 0x17;  // char ExcellentOption
    static constexpr int OFF_ANCIENT     = 0x18;  // char AncientOption
    static constexpr int OFF_REQSTR       = 0x19;  // short ReqStrength
    static constexpr int OFF_REQAGI       = 0x1B;  // short ReqDexterity
    static constexpr int OFF_REQENE       = 0x1D;  // short ReqEnergy
    static constexpr int OFF_REQVIT       = 0x1F;  // short ReqVitality
    static constexpr int OFF_REQCMD       = 0x21;  // short ReqCommand (DL)
    static constexpr int OFF_REQLEVEL     = 0x23;  // short ReqLevel
    static constexpr int OFF_SPECIALCOUNT = 0x25;  // char SpecialCount
    static constexpr int OFF_SPECIALTYPE  = 0x26;  // WORD[8] SpecialType
    static constexpr int OFF_SPECIALVALUE = 0x36;  // BYTE[8] SpecialValue
    static constexpr int OFF_UNIQUEID     = 0x3E;  // int UniqueID
    static constexpr int OFF_HARMONYTYPE  = 0x45;  // WORD HarmonyType
    static constexpr int OFF_HARMONYVALUE = 0x47;  // short HarmonyValue
    static constexpr int OFF_IS380        = 0x49;  // char Is380Item
    static constexpr int OFF_SOCKETOPT    = 0x4A;  // char[5] SocketOption
    static constexpr int OFF_SOCKETSEED   = 0x50;  // char[5] SocketSeedIndex
    static constexpr int OFF_SOCKETSPHERE = 0x55;  // char[5] SocketSphereLevel
    static constexpr int OFF_DURASTATE    = 0x60;  // char DurabilityState
    static constexpr int OFF_PERIOD       = 0x61;  // char PeriodItem
    static constexpr int OFF_EXPIRED      = 0x62;  // char ExpiredItem
    static constexpr int OFF_EXPIREDATE   = 0x63;  // int ExpireDateConvert

    static constexpr int SIZE = 107;
};