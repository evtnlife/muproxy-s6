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
//
// Organizado em três grupos (D5):
//   Globals — bases dos módulos e valores globais do main.exe
//   Object  — objetos do viewport (ObjectPreview) e o ObjectManager
//   Player  — jogador local (objeto do viewport + Character/UserObject)
// ============================================================================

namespace GameMemory {

// ============================================================================
// Globals — bases dos módulos e globals do main.exe
// ============================================================================
namespace Globals {

// Resolve base do main.exe (cache interno)
uintptr_t MainBase();

// Resolve base do Main.dll (cache interno)
uintptr_t MainDllBase();

// Número do mapa atual
inline DWORD MapNumber() {
    uintptr_t base = MainBase();
    return base ? *(DWORD*)(base + GameAddresses::MAP_NUMBER) : 0;
}

// Índice de entidade do alvo atual (global 0x081C03A4).
// O jogo grava o alvo aqui quando o jogador clica num monstro — é o que o
// jogo coloca direto no pacote de skill attack (sem XOR).
// NOTE: 0x081C03A4 é um endereço ABSOLUTO em main.exe, não um RVA.
inline uint16_t TargetEntityIndex() {
    uintptr_t addr = GameAddresses::TARGET_ENTITY_INDEX_GLOBAL;
    return addr ? *(uint16_t*)addr : 0;
}

// Índice de slot do alvo atual (global 0x081C03A0). -1 = nenhum alvo.
inline int32_t TargetSlotIndex() {
    uintptr_t addr = GameAddresses::TARGET_SLOT_GLOBAL;
    return addr ? *(int32_t*)addr : -1;
}

} // namespace Globals

// ============================================================================
// Object — objetos do viewport (ObjectPreview) e o ObjectManager
// ============================================================================
namespace Object {

// Ponteiro para o Object Manager (ou null)
uintptr_t Manager();

// Ponteiro para o array de objetos (ou null)
uintptr_t Array();

// Ponteiro para o objeto no index (ou null se inválido)
uintptr_t ByIndex(int index);

// Busca objeto pelo entity index (OFF_INDEX) — varre o array
inline uintptr_t FindByEntityIndex(uint16_t entityIndex) {
    uintptr_t arrayBase = Array();
    if (!arrayBase) return 0;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        if (*(uint16_t*)(obj + GameObject::OFF_INDEX) == entityIndex)
            return obj;
    }
    return 0;
}

// ---- Leitura tipada inline ----

inline BYTE Type(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(BYTE*)(obj + GameObject::OFF_TYPE) : 0;
}

inline BYTE IsLive(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(BYTE*)(obj + GameObject::OFF_ISLIVE) : 0;
}

inline float X(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(float*)(obj + GameObject::OFF_X) : 0.0f;
}

inline float Y(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(float*)(obj + GameObject::OFF_Y) : 0.0f;
}

inline float Z(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(float*)(obj + GameObject::OFF_Z) : 0.0f;
}

inline WORD CompositeIndex(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(WORD*)(obj + GameObject::OFF_INDEX) : 0;
}

// Retorna o OFF_ID (0x84) do objeto no slot — pode ser diferente do composite index
inline uint16_t Id(int index) {
    uintptr_t obj = ByIndex(index);
    return obj ? *(WORD*)(obj + GameObject::OFF_ID) : 0;
}

// Nome do objeto no slot
inline int Name(int index, char* buf, int maxLen) {
    uintptr_t obj = ByIndex(index);
    if (!obj || maxLen <= 0) { if (buf && maxLen > 0) buf[0] = '\0'; return 0; }
    const char* src = (const char*)(obj + GameObject::OFF_NAME);
    int i;
    for (i = 0; i < maxLen - 1 && src[i]; ++i) buf[i] = src[i];
    buf[i] = '\0';
    return i;
}

// AttackTargetID = EntityIndex XOR 0xEFF5
inline uint16_t ToAttackId(uint16_t entityIndex) {
    return entityIndex ^ 0xEFF5;
}

} // namespace Object

// ============================================================================
// Player — jogador local
// ============================================================================
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
