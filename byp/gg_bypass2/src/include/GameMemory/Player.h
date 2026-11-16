#pragma once
#include "sdk.h"
#include <cstdint>
#include "UserObjectStruct.h"

// ============================================================================
// GameMemory::Player — Jogador local
//
// Três estruturas distintas:
//   Object()     — o objeto do jogador no viewport (ObjectPreview)
//   Character()  — CharacterStruct (nome, classe, skills)
//   UserObject() — UserObjectStruct (level, stats, equipment)
// ============================================================================

namespace GameMemory {
namespace Player {

// Ponteiro para o objeto do jogador local no viewport (ou null)
uintptr_t Object();

// Ponteiro para a estrutura do personagem (ou null)
// Endereço: main.exe + GameAddresses::CHARACTER_PTR (struct direta)
// Contém: nome, classe, skills. NÃO contém level/stats.
uintptr_t Character();

// Ponteiro para a estrutura de stats do personagem (ou null)
// Endereço: *(main.exe + GameAddresses::USER_OBJECT_PTR) (ponteiro indireto)
// Contém: nome, level, stats (STR/AGI/VIT/ENE)
uintptr_t UserObject();

// Define o index do jogador local (recebido do proxy via D4).
// Quando definido, Object() busca por type==1 && index==playerIndex.
void SetIndex(uint16_t index);
uint16_t Index();

// ---- Stats (UserObjectStruct) ----

inline WORD Level() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_LEVEL) : 0;
}

inline WORD Str() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_STR) : 0;
}
inline WORD Agi() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_AGI) : 0;
}
inline WORD Vit() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_VIT) : 0;
}
inline WORD Ene() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_ENE) : 0;
}
inline WORD Life() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_LIFE) : 0;
}
inline WORD MaxLife() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MAXLIFE) : 0;
}
inline WORD Mana() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MANA) : 0;
}
inline WORD MaxMana() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MAXMANA) : 0;
}
inline WORD Shield() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_SHIELD) : 0;
}
inline WORD MaxShield() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MAXSHIELD) : 0;
}
inline WORD AttackSpeed() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_ATTACKSPEED) : 0;
}
inline WORD DamageMin() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_DAMAGEMIN) : 0;
}
inline WORD DamageMax() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_DAMAGEMAX) : 0;
}
inline WORD LevelPoint() {
    uintptr_t uo = UserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_LEVELPOINT) : 0;
}
inline DWORD Exp() {
    uintptr_t uo = UserObject();
    return uo ? *(DWORD*)(uo + UserObjectStruct::OFF_EXP) : 0;
}
inline DWORD NextExp() {
    uintptr_t uo = UserObject();
    return uo ? *(DWORD*)(uo + UserObjectStruct::OFF_NEXTEXP) : 0;
}
inline DWORD Money() {
    uintptr_t uo = UserObject();
    return uo ? *(DWORD*)(uo + UserObjectStruct::OFF_MONEY_INV) : 0;
}
inline BYTE Class() {
    uintptr_t uo = UserObject();
    return uo ? *(BYTE*)(uo + UserObjectStruct::OFF_CLASS) : 0;
}
inline WORD MagicCount() {
    uintptr_t uo = UserObject();
    return uo ? *(BYTE*)(uo + UserObjectStruct::OFF_MAGICCOUNT) : 0;
}

// Ponteiro para a lista de magias (WORD[650])
inline uintptr_t MagicList() {
    uintptr_t uo = UserObject();
    return uo ? (uo + UserObjectStruct::OFF_MAGICLIST) : 0;
}

// Ponteiro para um item do equipamento (slot 0-11)
inline uintptr_t Equipment(int slot) {
    if (slot < 0 || slot > 11) return 0;
    uintptr_t uo = UserObject();
    return uo ? (uo + UserObjectStruct::OFF_EQUIPMENT + slot * UserObjectStruct::ITEM_SIZE) : 0;
}

} // namespace Player
} // namespace GameMemory
