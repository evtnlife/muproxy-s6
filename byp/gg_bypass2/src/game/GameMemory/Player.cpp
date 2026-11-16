// game/GameMemory/Player.cpp — Jogador local (objeto do viewport, character e stats)
#include "GameMemory/Player.h"
#include "GameMemory/Object.h"
#include "GameMemory/Globals.h"
#include "GameAddresses.h"
#include "GameObject.h"
#include "CharacterStruct.h"
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
                BYTE type = *(BYTE*)(obj + GameObject::OFF_TYPE);
                BYTE live = *(BYTE*)(obj + GameObject::OFF_ISLIVE);
                WORD idx = *(WORD*)(obj + GameObject::OFF_INDEX);
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
            BYTE type = *(BYTE*)(ptr + GameObject::OFF_TYPE);
            BYTE live = *(BYTE*)(ptr + GameObject::OFF_ISLIVE);
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
    // Valida: verifica se o nome é ASCII imprimível
    const char* name = (const char*)(ptr + CharacterStruct::OFF_NAME);
    bool valid = true;
    for (int i = 0; i < 10 && name[i]; i++) {
        if (name[i] < 0x20 || name[i] > 0x7E) { valid = false; break; }
    }
    if (valid && name[0] != '\0') return ptr;
    return 0;
}

uintptr_t UserObject() {
    uintptr_t base = Globals::MainBase();
    if (!base) return 0;
    // USER_OBJECT_PTR é um ponteiro indireto: *(base + RVA)
    uintptr_t ptr = *(uintptr_t*)(base + GameAddresses::USER_OBJECT_PTR);
    if (!ptr) return 0;
    // Valida: verifica se o nome é ASCII imprimível
    const char* name = (const char*)(ptr + UserObjectStruct::OFF_NAME);
    bool valid = true;
    for (int i = 0; i < 10 && name[i]; i++) {
        if (name[i] < 0x20 || name[i] > 0x7E) { valid = false; break; }
    }
    if (valid && name[0] != '\0') return ptr;
    return 0;
}

} // namespace Player
} // namespace GameMemory
