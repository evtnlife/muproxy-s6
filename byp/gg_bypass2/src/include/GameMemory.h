#pragma once
#include "sdk.h"
#include <cstdint>
#include "GameAddresses.h"
#include "GameObject.h"
#include "CharacterStruct.h"
#include "UserObjectStruct.h"
#include "ObjType.h"

// ============================================================================
// GameMemory — Leitura de estruturas do MU Online client em memória
//
// Toda a comunicação com a memória do processo passa por aqui.
// Endereços base são relativos ao módulo main.exe e resolvidos em runtime.
// ============================================================================

namespace GameMemory {

// Resolve base do main.exe (cache interno)
uintptr_t GetMainBase();

// Resolve base do Main.dll (cache interno)
uintptr_t GetMainDllBase();

// Retorna ponteiro para o Object Manager (ou null)
uintptr_t GetObjectManager();

// Retorna ponteiro para o array de objetos (ou null)
uintptr_t GetObjectArray();

// Retorna ponteiro para o objeto no index (ou null se inválido)
uintptr_t GetObjectByIndex(int index);

// Retorna ponteiro para o local player (ou null)
uintptr_t GetLocalPlayer();

// Retorna ponteiro para a estrutura do personagem (ou null)
// Endereço: main.exe + GameAddresses::CHARACTER_PTR (struct direta)
// Contém: nome, classe, skills. NÃO contém level/stats.
uintptr_t GetCharacter();

// Retorna ponteiro para a estrutura de stats do personagem (ou null)
// Endereço: *(main.exe + GameAddresses::USER_OBJECT_PTR) (ponteiro indireto)
// Contém: nome, level, stats (STR/AGI/VIT/ENE)
uintptr_t GetUserObject();

// Define o index do jogador local (recebido do proxy via D4)
// Quando definido, GetLocalPlayer() busca por type==1 && index==playerIndex
void SetLocalPlayerIndex(uint16_t index);
uint16_t GetLocalPlayerIndex();

// ---- Leitura tipada inline ----

inline BYTE GetType(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(BYTE*)(obj + GameObject::OFF_TYPE) : 0;
}

inline BYTE IsLive(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(BYTE*)(obj + GameObject::OFF_ISLIVE) : 0;
}

inline float GetX(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(float*)(obj + GameObject::OFF_X) : 0.0f;
}

inline float GetY(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(float*)(obj + GameObject::OFF_Y) : 0.0f;
}

inline float GetZ(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(float*)(obj + GameObject::OFF_Z) : 0.0f;
}

inline WORD GetCompositeIndex(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(WORD*)(obj + GameObject::OFF_INDEX) : 0;
}

// Retorna o OFF_ID (0x84) do objeto no slot — pode ser diferente do composite index
inline uint16_t GetObjectId(int index) {
    uintptr_t obj = GetObjectByIndex(index);
    return obj ? *(WORD*)(obj + GameObject::OFF_ID) : 0;
}

// Busca objeto pelo entity index (OFF_INDEX) — varre o array
inline uintptr_t FindObjectByEntityIndex(uint16_t entityIndex) {
    uintptr_t arrayBase = GetObjectArray();
    if (!arrayBase) return 0;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        if (*(uint16_t*)(obj + GameObject::OFF_INDEX) == entityIndex)
            return obj;
    }
    return 0;
}

// AttackTargetID = EntityIndex XOR 0xEFF5
inline uint16_t EntityToAttackId(uint16_t entityIndex) {
    return entityIndex ^ 0xEFF5;
}

// Read the game's current target entity index from global 0x081C03A4
// The game stores the target here when the player clicks on a monster.
// This is what the game puts directly in skill attack packets (no XOR).
// NOTE: 0x081C03A4 is an ABSOLUTE address in main.exe, NOT an RVA.
inline uint16_t GetTargetGlobal() {
    uintptr_t addr = GameAddresses::TARGET_ENTITY_INDEX_GLOBAL;
    return addr ? *(uint16_t*)addr : 0;
}

// Read the game's current target slot index from global 0x081C03A0
// -1 means no target selected
inline int32_t GetTargetSlotGlobal() {
    uintptr_t addr = GameAddresses::TARGET_SLOT_GLOBAL;
    return addr ? *(int32_t*)addr : -1;
}

// Level do personagem local (da UserObjectStruct)
inline WORD GetPlayerLevel() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_LEVEL) : 0;
}

// Stats do personagem local (da UserObjectStruct)
inline WORD GetPlayerStr() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_STR) : 0;
}
inline WORD GetPlayerAgi() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_AGI) : 0;
}
inline WORD GetPlayerVit() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_VIT) : 0;
}
inline WORD GetPlayerEne() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_ENE) : 0;
}
inline WORD GetPlayerLife() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_LIFE) : 0;
}
inline WORD GetPlayerMaxLife() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MAXLIFE) : 0;
}
inline WORD GetPlayerMana() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MANA) : 0;
}
inline WORD GetPlayerMaxMana() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MAXMANA) : 0;
}
inline WORD GetPlayerShield() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_SHIELD) : 0;
}
inline WORD GetPlayerMaxShield() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_MAXSHIELD) : 0;
}
inline WORD GetPlayerAttackSpeed() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_ATTACKSPEED) : 0;
}
inline WORD GetPlayerDamageMin() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_DAMAGEMIN) : 0;
}
inline WORD GetPlayerDamageMax() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_DAMAGEMAX) : 0;
}
inline WORD GetPlayerLevelPoint() {
    uintptr_t uo = GetUserObject();
    return uo ? *(WORD*)(uo + UserObjectStruct::OFF_LEVELPOINT) : 0;
}
inline DWORD GetPlayerExp() {
    uintptr_t uo = GetUserObject();
    return uo ? *(DWORD*)(uo + UserObjectStruct::OFF_EXP) : 0;
}
inline DWORD GetPlayerNextExp() {
    uintptr_t uo = GetUserObject();
    return uo ? *(DWORD*)(uo + UserObjectStruct::OFF_NEXTEXP) : 0;
}
inline DWORD GetPlayerMoney() {
    uintptr_t uo = GetUserObject();
    return uo ? *(DWORD*)(uo + UserObjectStruct::OFF_MONEY_INV) : 0;
}
inline BYTE GetPlayerClass() {
    uintptr_t uo = GetUserObject();
    return uo ? *(BYTE*)(uo + UserObjectStruct::OFF_CLASS) : 0;
}
inline WORD GetPlayerMagicCount() {
    uintptr_t uo = GetUserObject();
    return uo ? *(BYTE*)(uo + UserObjectStruct::OFF_MAGICCOUNT) : 0;
}
// Retorna ponteiro para a lista de magias (WORD[650])
inline uintptr_t GetPlayerMagicList() {
    uintptr_t uo = GetUserObject();
    return uo ? (uo + UserObjectStruct::OFF_MAGICLIST) : 0;
}
// Retorna ponteiro para um item do equipamento (slot 0-11)
inline uintptr_t GetPlayerEquipment(int slot) {
    if (slot < 0 || slot > 11) return 0;
    uintptr_t uo = GetUserObject();
    return uo ? (uo + UserObjectStruct::OFF_EQUIPMENT + slot * UserObjectStruct::ITEM_SIZE) : 0;
}

inline int GetName(int index, char* buf, int maxLen) {
    uintptr_t obj = GetObjectByIndex(index);
    if (!obj || maxLen <= 0) { if (buf && maxLen > 0) buf[0] = '\0'; return 0; }
    const char* src = (const char*)(obj + GameObject::OFF_NAME);
    int i;
    for (i = 0; i < maxLen - 1 && src[i]; ++i) buf[i] = src[i];
    buf[i] = '\0';
    return i;
}

inline DWORD GetMapNumber() {
    uintptr_t base = GetMainBase();
    return base ? *(DWORD*)(base + GameAddresses::MAP_NUMBER) : 0;
}

} // namespace GameMemory