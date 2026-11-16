#pragma once
#include "sdk.h"
#include <cstdint>
#include "GameAddresses.h"
#include "GameObject.h"

// ============================================================================
// GameMemory::Object — Objetos do viewport (ObjectPreview) e o ObjectManager
//
// O ObjectManager é o array de objetos do mundo visível (400 slots), cada um
// com um ObjectPreview (tipo, vivo, índice, posição, nome).
// ============================================================================

namespace GameMemory {
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
} // namespace GameMemory
