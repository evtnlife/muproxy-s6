#pragma once
#include <cstdint>

// ============================================================================
// PartyList — Offsets de um membro da party (MuEmu Object.h, 32 bytes)
//
// Endereço: *(main.exe + 0x81CB4E8) + slot * 32
// ============================================================================

struct PartyList {
    static constexpr int OFF_NAME       = 0x00;  // char[10] nome
    static constexpr int OFF_MAPNUMBER  = 0x0C;  // byte MapNumber
    static constexpr int OFF_POSX       = 0x0D;  // byte PosX
    static constexpr int OFF_POSY       = 0x0E;  // byte PosY
    static constexpr int OFF_LIFE       = 0x10;  // int Life
    static constexpr int OFF_MAXLIFE    = 0x14;  // int MaxLife
    static constexpr int OFF_LIFEPCT    = 0x18;  // byte LifePercent
    static constexpr int OFF_VIEWPORTID = 0x1C;  // int ViewportID

    static constexpr int SIZE = 32;
};