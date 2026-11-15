#pragma once
#include <cstdint>
#include "ObjType.h"

// ============================================================================
// GameObject — Offsets do ObjectPreview (MuEmu Object.h, 1432 bytes)
//
// Descobertos via engenharia reversa com x32dbg + Ghidra.
// O array base é: [[main.exe + 0xA7C224] + 0x8] + index * 0x598
//
// Para ler um campo: *(tipo*)(objBase + OFF_xxx)
// Ex: BYTE type = *(BYTE*)(objBase + GameObject::OFF_TYPE);
// ============================================================================

struct GameObject {
    // ---- Cabeçalho (ObjectPreview) ----
    static constexpr int OFF_SAFEZONE  = 0x0E;  // bool InSafeZone
    static constexpr int OFF_ISLIVE    = 0x30C; // byte IsLiveObject (1=alive) — CONFIRMADO no BlackRock
    static constexpr int OFF_CLASS     = 0x13;  // byte Class
    static constexpr int OFF_CTLCODE   = 0x15;  // byte CtlCode (0x10 = GM)
    static constexpr int OFF_GENS      = 0x1F;  // byte GensInfluence (0=None,1=D,2=V)
    static constexpr int OFF_NAME      = 0x38;  // char[25] nome
    static constexpr int OFF_INDEX     = 0x7E;  // word aIndex (composite)
    static constexpr int OFF_ID        = 0x84;  // word ID

    // ---- Posição (ObjectPreview) ----
    static constexpr int OFF_MAPPOSX   = 0xAC;  // dword MapPosX
    static constexpr int OFF_MAPPOSY   = 0xB0;  // dword MapPosY

    // ---- Equipment visual (ObjectPreview) ----
    static constexpr int OFF_HELM      = 0x10C; // short HelmSlot
    static constexpr int OFF_ARMOR     = 0x130; // short ArmorSlot
    static constexpr int OFF_PANTS     = 0x154; // short PantsSlot
    static constexpr int OFF_GLOVES    = 0x178; // short GlovesSlot
    static constexpr int OFF_BOOTS     = 0x19C; // short BootsSlot
    static constexpr int OFF_WEAPON1   = 0x1C0; // short WeaponFirstSlot
    static constexpr int OFF_WEAPON2   = 0x1E4; // short WeaponSecondSlot
    static constexpr int OFF_WINGS     = 0x208; // short WingsSlot
    static constexpr int OFF_PET       = 0x22C; // short PetSlot

    // ---- Model (ObjectPreview) ----
    static constexpr int OFF_MODEL     = 0x308; // ObjectModel (648 bytes)

    // ---- Posição world (descoberto via x32dbg) ----
    static constexpr int OFF_X         = 0x404; // float world X
    static constexpr int OFF_Y         = 0x408; // float world Y
    static constexpr int OFF_Z         = 0x40C; // float world Z

    // ---- Offsets de ataque (descoberto via RE do Main.exe) ----
    static constexpr int OFF_ATK_PX    = 0xFC;  // float player X (ataque)
    static constexpr int OFF_ATK_PY    = 0x100; // float player Y (ataque)
    static constexpr int OFF_ATK_TX    = 0xD0;  // float target X (ataque)
    static constexpr int OFF_ATK_TY    = 0xD4;  // float target Y (ataque)
    static constexpr int OFF_ATK_AC    = 0xAC;  // dword action byte field
    static constexpr int OFF_ATK_B0    = 0xB0;  // dword action byte field
    static constexpr int OFF_DIR_ANGLE = 0x110; // float direction angle

    // ---- Tipo (descoberto via x32dbg) ----
    static constexpr int OFF_TYPE      = 0x320; // byte (ObjType enum)

    // ---- Constantes ----
    static constexpr int SLOT_SIZE     = 0x598; // 1432 bytes
};