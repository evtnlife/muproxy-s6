// game/GameMemory/Player.cpp — Jogador local (objeto do viewport, character e stats)
#include "GameMemory/Player.h"
#include "GameMemory/Object.h"
#include "GameMemory/Globals.h"
#include "GameAddresses.h"
#include "GameObject.h"
#include "CharacterStruct.h"
#include "PartyList.h"
#include "SafeRead.h"
#include "ObjType.h"
#include "Logger.h"

namespace GameMemory {
namespace Player {

static uint16_t g_LocalPlayerIndex = 0;  // Index do jogador local (setado via D4)

void SetIndex(uint16_t index) {
    g_LocalPlayerIndex = index;
    Logger::Log("GAMEMEMORY", "LocalPlayerIndex setado para 0x%04X", index);
}

uint16_t Index() {
    return g_LocalPlayerIndex;
}

uintptr_t Object() {
    // Metodo 1: Se o PlayerIndex foi setado via D4, busca direto pelo slot
    // com type==1 e index==playerIndex. Isso e 100% preciso mesmo com
    // multiplos players na tela.
    if (g_LocalPlayerIndex != 0) {
        uintptr_t arrayBase = GameMemory::Object::Array();
        if (arrayBase) {
            for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
                uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
                BYTE type = SafeRead::ReadByte((const void*)(obj + GameObject::OFF_TYPE));
                BYTE live = SafeRead::ReadByte((const void*)(obj + GameObject::OFF_ISLIVE));
                WORD idx = SafeRead::ReadWord((const void*)(obj + GameObject::OFF_INDEX));
                if (type == ObjType::OBJ_PLAYER && live && idx == g_LocalPlayerIndex) {
                    return obj;
                }
            }
            // Index nao encontrado (player pode ter mudado de mapa), limpa cache
            static int lastNotFoundLog = 0;
            int now = GetTickCount();
            if (now - lastNotFoundLog > 5000) { // loga no maximo a cada 5s
                lastNotFoundLog = now;
                Logger::Log("GAMEMEMORY", "LocalPlayerIndex 0x%04X nao encontrado, fallback para scan", g_LocalPlayerIndex);
            }
        }
    }

    // Metodo 2: Tenta o ponteiro direto
    uintptr_t base = Globals::MainBase();
    if (base) {
        uintptr_t ptr = *(uintptr_t*)(base + GameAddresses::LOCAL_PLAYER_PTR);
        if (ptr) {
            BYTE type = SafeRead::ReadByte((const void*)(ptr + GameObject::OFF_TYPE));
            BYTE live = SafeRead::ReadByte((const void*)(ptr + GameObject::OFF_ISLIVE));
            if (type == ObjType::OBJ_PLAYER && live) {
                return ptr;
            }
        }
    }

    // Metodo 3: Scan do ObjectArray — retorna o primeiro type=1 e isLive!=0
    uintptr_t arrayBase = GameMemory::Object::Array();
    if (!arrayBase) return 0;

    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        BYTE type = *(BYTE*)(obj + GameObject::OFF_TYPE);
        BYTE live = *(BYTE*)(obj + GameObject::OFF_ISLIVE);
        if (type == ObjType::OBJ_PLAYER && live) {
            static int lastSlot = -1;
            if (i != lastSlot) {
                lastSlot = i;
                Logger::Log("GAMEMEMORY", "GetLocalPlayer: fallback scan encontrou player no slot %d idx=0x%04X", i, *(WORD*)(obj + GameObject::OFF_INDEX));
            }
            return obj;
        }
    }

    return 0;
}

uintptr_t Character() {
    uintptr_t base = Globals::MainBase();
    if (!base) return 0;
    // CHARACTER_PTR é o endereço DIRETO da struct, não um ponteiro
    uintptr_t ptr = base + GameAddresses::CHARACTER_PTR;
    // Nome ASCII (leitura segura: o __try sozinho NÃO protege loads — o /O2
    //  pode movê-los para fora da região protegida).
    char name[16];
    if (!SafeRead::ReadAsciiName((const char*)(ptr + CharacterStruct::OFF_NAME), name, 11)) {
        return 0;
    }
    return ptr;
}

uintptr_t UserObject() {
    uintptr_t base = Globals::MainBase();
    if (!base) return 0;
    // USER_OBJECT_PTR é ponteiro indireto (oUserObjectStruct).
    // ⚠️ No BlackRock esse endereço é a tabela de skill wire IDs, não o
    // ObjectCharacter → o ponteiro lido é lixo e precisa ser validado.
    uintptr_t ptr = SafeRead::ReadPointer((const void*)(base + GameAddresses::USER_OBJECT_PTR));
    if (!ptr) return 0;

    // O ponteiro tem que estar dentro de algum módulo carregado…
    HMODULE mod = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            (LPCSTR)ptr, &mod)) {
        return 0;
    }
    // …e o conteúdo tem que ser o nome do personagem (ASCII).
    char name[16];
    if (!SafeRead::ReadAsciiName((const char*)(ptr + UserObjectStruct::OFF_NAME), name, 11)) {
        return 0;
    }
    return ptr;
}

// ---- Leitura segura dos stats (o ponteiro pode virar lixo a qualquer hora) ----
WORD ReadStatWord(uintptr_t base, int offset)
{
    if (!base) return 0;
    return SafeRead::ReadWord((const void*)(base + offset));
}

WORD Life()      { return ReadStatWord(UserObject(), UserObjectStruct::OFF_LIFE); }
WORD MaxLife()   { return ReadStatWord(UserObject(), UserObjectStruct::OFF_MAXLIFE); }
WORD Mana()      { return ReadStatWord(UserObject(), UserObjectStruct::OFF_MANA); }
WORD MaxMana()   { return ReadStatWord(UserObject(), UserObjectStruct::OFF_MAXMANA); }
WORD Level()     { return ReadStatWord(UserObject(), UserObjectStruct::OFF_LEVEL); }
WORD Str()       { return ReadStatWord(UserObject(), UserObjectStruct::OFF_STR); }
WORD Agi()       { return ReadStatWord(UserObject(), UserObjectStruct::OFF_AGI); }
WORD Vit()       { return ReadStatWord(UserObject(), UserObjectStruct::OFF_VIT); }
WORD Ene()       { return ReadStatWord(UserObject(), UserObjectStruct::OFF_ENE); }
WORD Shield()    { return ReadStatWord(UserObject(), UserObjectStruct::OFF_SHIELD); }
WORD MaxShield() { return ReadStatWord(UserObject(), UserObjectStruct::OFF_MAXSHIELD); }
WORD AttackSpeed() { return ReadStatWord(UserObject(), UserObjectStruct::OFF_ATTACKSPEED); }
WORD DamageMin() { return ReadStatWord(UserObject(), UserObjectStruct::OFF_DAMAGEMIN); }
WORD DamageMax() { return ReadStatWord(UserObject(), UserObjectStruct::OFF_DAMAGEMAX); }
WORD LevelPoint(){ return ReadStatWord(UserObject(), UserObjectStruct::OFF_LEVELPOINT); }

DWORD ReadStatDword(uintptr_t base, int offset)
{
    if (!base) return 0;
    return SafeRead::ReadDword((const void*)(base + offset));
}

BYTE ReadStatByte(uintptr_t base, int offset)
{
    if (!base) return 0;
    return SafeRead::ReadByte((const void*)(base + offset));
}

DWORD Exp()      { return ReadStatDword(UserObject(), UserObjectStruct::OFF_EXP); }
DWORD NextExp()  { return ReadStatDword(UserObject(), UserObjectStruct::OFF_NEXTEXP); }
DWORD Money()    { return ReadStatDword(UserObject(), UserObjectStruct::OFF_MONEY_INV); }
BYTE  Class()    { return ReadStatByte(UserObject(), UserObjectStruct::OFF_CLASS); }
WORD  MagicCount(){ return ReadStatByte(UserObject(), UserObjectStruct::OFF_MAGICCOUNT); }

// ============================================================================
// PartyList — o jogador local é o slot 0 da party list
// (Life @0x10, MaxLife @0x14, LifePercent @0x18).
// Ponteiro: *(main.exe + 0x81CB4E8) + slot * 32
// ============================================================================
uintptr_t PartyMember(int slot) {
    uintptr_t base = Globals::MainBase();
    if (!base || slot < 0 || slot > 4) return 0;
    uintptr_t list = SafeRead::ReadPointer((const void*)(base + GameAddresses::PARTY_LIST_PTR));
    if (!list) return 0;
    uintptr_t member = list + (uintptr_t)slot * PartyList::SIZE;
    if (!SafeRead::IsReadable((const void*)member, PartyList::SIZE)) return 0;
    return member;
}

int PartyLife(int slot) {
    uintptr_t p = PartyMember(slot);
    if (!p) return 0;
    return SafeRead::ReadInt((const void*)(p + PartyList::OFF_LIFE));
}

int PartyMaxLife(int slot) {
    uintptr_t p = PartyMember(slot);
    if (!p) return 0;
    return SafeRead::ReadInt((const void*)(p + PartyList::OFF_MAXLIFE));
}

int PartyLifePercent(int slot) {
    uintptr_t p = PartyMember(slot);
    if (!p) return -1;
    return SafeRead::ReadByte((const void*)(p + PartyList::OFF_LIFEPCT));
}

} // namespace Player
} // namespace GameMemory
