#pragma once
#include <cstdint>

// ============================================================================
// GameAddresses — Endereços estáticos do main.exe (RVAs)
//
// Estes offsets são compilados no binário e NÃO mudam entre sessões
// na mesma versão do cliente. Para usar em runtime:
//   uintptr_t addr = GetModuleHandle("main.exe") + RVA;
//
// Encontrados via engenharia reversa com x32dbg + Ghidra.
// Documentação completa em docs/PS_Object_Array.md
// ============================================================================

struct GameAddresses {
    // Object array access chain:
    //   manager = *[base + OBJ_MANAGER_PTR]
    //   array   = *[manager + OBJ_ARRAY_OFFSET]
    //   object  = array + index * OBJECT_SIZE
    static constexpr uintptr_t OBJ_MANAGER_PTR  = 0xA7C224;  // DWORD* -> Object Manager
    static constexpr uintptr_t OBJ_ARRAY_OFFSET = 0x8;       // offset inside Object Manager
    static constexpr uintptr_t LOCAL_PLAYER_PTR  = 0x7BC4F04; // DWORD* -> local player GameObject
    static constexpr uintptr_t CHARACTER_PTR     = 0xA96B0C;  // struct direta: nome, classe, skills (NÃO tem level!)
    static constexpr uintptr_t USER_OBJECT_PTR    = 0x8128AC8; // DWORD* -> user object struct (nome, level, stats)
    static constexpr uintptr_t MAP_NUMBER        = 0xA61E18; // DWORD (current map id)
    static constexpr uintptr_t VIEWPORT_WIDTH    = 0xA61E58; // DWORD (pixels)
    static constexpr uintptr_t VIEWPORT_HEIGHT   = 0xA61E5C; // DWORD (pixels)

    // Limits
    static constexpr int MAX_OBJECTS  = 400;
    static constexpr int OBJECT_SIZE  = 0x598;   // 1432 bytes per slot

    // Skill attack builder functions (main.exe RVAs)
    // Base = 0x00400000, absolute = base + RVA

    // SKILL_ENCRYPT_SEND: encrypt+send the packet
    // __thiscall: ECX = struct* { uint16_t size; uint8_t buf[]; }
    // Stack args: (int arg1=1, int arg2=0)
    // Absolute: 0x00404D70
    static constexpr uintptr_t SKILL_ENCRYPT_SEND = 0x4D70;

    // SKILL_BUILDER_INIT: initialize builder object (0x81C bytes)
    // __thiscall: ECX = builder object pointer
    // Absolute: 0x00404B40
    static constexpr uintptr_t SKILL_BUILDER_INIT = 0x4B40;

    // SKILL_BUILDER_SET_HEADER: set header and opcode
    // __thiscall: ECX = builder, push opcode, push header
    // Absolute: 0x00404B90
    static constexpr uintptr_t SKILL_BUILDER_SET_HEADER = 0x4B90;

    // SKILL_BUILDER_ADD_DATA: add data bytes to packet
    // __thiscall: ECX = builder/slot, 1 stack arg consumed per call
    // Called 4x chained: push skillH, skillL, targetH, targetL
    // Each call: ret 0x04 (cleans 1 arg), returns this for chaining
    // Absolute: 0x004050D0
    static constexpr uintptr_t SKILL_BUILDER_ADD_DATA = 0x50D0;

    // SKILL_BUILDER_DESTROY: destroy builder object
    // __thiscall: ECX = builder
    // Absolute: 0x00404B60
    static constexpr uintptr_t SKILL_BUILDER_DESTROY = 0x4B60;

    // Global addresses for target entity index (used by skill attack function)
    // These are ABSOLUTE addresses in main.exe (NOT RVAs).
    // 0x081C03A0: DWORD — selected target slot (-1 if none)
    // 0x081C03A4: DWORD — target entity index / aIndex (used in skill packets)
    // The game reads: mov edx, [0x081C03A4]; and edx, 0xFF (low byte)
    //                mov eax, [0x081C03A4]; sar eax, 8 (high byte)
    // This is the target ID that goes DIRECTLY into the skill attack packet (no XOR).
    static constexpr uintptr_t TARGET_SLOT_GLOBAL     = 0x081C03A0;
    static constexpr uintptr_t TARGET_ENTITY_INDEX_GLOBAL = 0x081C03A4;

    // SimpleModulus::Encrypt — game's internal SimpleModulus encryption
    // __thiscall: ECX = SimpleModulus object pointer (contains keys)
    // Stack args: (void* dest, const void* source, int size)
    // Returns: int (encrypted size)
    // RVA: 0x598420, Absolute: 0x00998420 (confirmed via x64dbg at 0x404E78: call 0x00998420)
    static constexpr uintptr_t SIMPLEMODULUS_ENCRYPT = 0x598420;

    // SimpleModulus::Decrypt — game's internal SimpleModulus decryption
    // __thiscall: ECX = SimpleModulus object pointer
    // Stack args: (void* dest, const void* source, int size)
    // Returns: int (decrypted size)
    // RVA: 0x598490, Absolute: 0x00998490 (confirmed via x32dbg: call 0x998620 per block)
    static constexpr uintptr_t SIMPLEMODULUS_DECRYPT = 0x598490;

    // Skill wire ID table (builder 0x59C260, prologue)
    // byte [SKILL_TABLE_FLAG] != 0 → table active, use lookup
    // dword [SKILL_TABLE_INDEX] = raw skill index (e.g. 0x3C for Force)
    // dword [SKILL_TABLE_BASE] = base pointer of the wire ID table
    // word [SKILL_TABLE_BASE + index*2 + 0x78] = wire skill ID (16-bit)
    // Absolute addresses (NOT RVAs).
    static constexpr uintptr_t SKILL_TABLE_FLAG  = 0x081C0398;
    static constexpr uintptr_t SKILL_TABLE_INDEX = 0x081C039C;
    static constexpr uintptr_t SKILL_TABLE_BASE  = 0x08128AC8;


};