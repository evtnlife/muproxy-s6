#pragma once
#include <cstdint>

// ============================================================================
// ObjType — Tipos de objeto no MU Online client
// Encontrado no offset +0x320 de cada slot do object array
// ============================================================================

enum ObjType : BYTE {
    OBJ_EMPTY   = 0,
    OBJ_PLAYER  = 1,
    OBJ_MONSTER = 2,
    OBJ_NPC     = 4,
};