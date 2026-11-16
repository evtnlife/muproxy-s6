#pragma once

#include <cstdint>

// ============================================================================
// MuOpcodes — Códigos de operação dos pacotes do MU Online
//
// Cada pacote começa com um header:
//   C1 XX YY      → tamanho 1 byte,  não-encrypted
//   C2 XX XX YY   → tamanho 2 bytes,  não-encrypted
//   C3 XX YY      → tamanho 1 byte,  encrypted
//   C4 XX XX YY   → tamanho 2 bytes,  encrypted
//
// O byte após o tamanho é o opcode (ou o sub-opcode, nos C3/C4 com prefixo F3).
//
// ⚠️ O espaço de opcodes do MU é DIRECIONAL: o MESMO valor pode existir nos
// dois sentidos com significados diferentes. Ex.: 0x26 = "uso de item"
// (cliente→servidor) e "update de life" (servidor→cliente). Por isso os
// opcodes são separados em duas enums — nunca misture um `ClientToServer`
// com um `ServerToClient`.
//
// Convenção de nomes: o membro da enum = o que o pacote FAZ.
// ============================================================================

namespace MuOpcodes {

using u8 = uint8_t;

// ---- Tipos de header ----
enum class HeaderType : u8 {
    C1 = 0xC1,   // tamanho 1 byte, não-encrypted
    C2 = 0xC2,   // tamanho 2 bytes, não-encrypted
    C3 = 0xC3,   // tamanho 1 byte, encrypted
    C4 = 0xC4,   // tamanho 2 bytes, encrypted
};

// ============================================================================
// Opcodes CLIENTE → SERVIDOR (é o que o byp.dll INJETA no game server)
// ============================================================================
enum class ClientToServer : u8 {
    // ---- Ataque ----
    Attack            = 0x11,  // C1 07 11 <targetHi> <targetLo> 0x78 <dir 0..7>
    TargetedSkill     = 0x19,  // C1 07 19 <skillH> <skillL> <indexH> <indexL>
    SkillAttackLegacy = 0xE5,  // C3 0D E5 + 10 bytes — NÃO implementado (legado)

    // ---- Itens ----
    ItemUse           = 0x26,  // C1 06 26 <slot> <targetSlot> <type>  (consumível: potion)
    TalkNpc           = 0x30,
    CreateCharacter   = 0x31,
    DeleteCharacter   = 0x32,
    SelectCharacter   = 0x33,
    HitRequest        = 0x3D,

    // ---- Movimento / magia ----
    Move              = 0xD3,
    MagicAttack       = 0xD7,
};

// ============================================================================
// Opcodes SERVIDOR → CLIENTE (é o que o byp.dll OBSERVA/decodifica)
// ============================================================================
enum class ServerToClient : u8 {
    MapChange     = 0x03,
    ObjectMove    = 0x11,
    ObjectAdd     = 0x12,
    ObjectList    = 0x13,
    ObjectAttack  = 0x14,
    DamageNumber  = 0x15,
    HpUpdate      = 0x16,
    ObjectRemove  = 0x18,
    LifeUpdate    = 0x26,  // C1 26 <type> <lifeHi> <lifeLo> <flag> <shieldHi> <shieldLo>
    ManaUpdate    = 0x27,  // C1 27 <type> <manaHi> <manaLo> <bpHi> <bpLo>
    SkillList     = 0xF3,  // F3 <sub> — lista de skills (sub 0x11 / 0xE2)
};

// ---- Tipos de ataque (campo "type" da sessão do módulo de ataque) ----
enum class AttackType : u8 {
    Normal = 0x00,
    Skill  = 0x01,
};

// ============================================================================
// Tamanhos de pacote (bytes do header inclusive)
// ============================================================================
namespace Size {
    constexpr u8 AttackNormal  = 7;   // C1 07 11 ...
    constexpr u8 TargetedSkill = 7;   // C1 07 19 ...
    constexpr u8 ItemUse       = 6;   // C1 06 26 ...
    constexpr u8 SkillAttack   = 13;  // C3 0D E5 ...
}

} // namespace MuOpcodes
