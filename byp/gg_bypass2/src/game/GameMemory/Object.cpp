// game/GameMemory/Object.cpp — ObjectManager e array de objetos do viewport
#include "GameMemory/Object.h"
#include "GameMemory/Globals.h"
#include "GameAddresses.h"

namespace GameMemory {
namespace Object {

uintptr_t Manager() {
    uintptr_t base = Globals::MainBase();
    if (!base) return 0;
    return *(uintptr_t*)(base + GameAddresses::OBJ_MANAGER_PTR);
}

uintptr_t Array() {
    uintptr_t manager = Manager();
    if (!manager) return 0;
    return *(uintptr_t*)(manager + GameAddresses::OBJ_ARRAY_OFFSET);
}

uintptr_t ByIndex(int index) {
    if (index < 0 || index >= GameAddresses::MAX_OBJECTS) return 0;
    uintptr_t arrayBase = Array();
    if (!arrayBase) return 0;
    return arrayBase + index * GameAddresses::OBJECT_SIZE;
}

} // namespace Object
} // namespace GameMemory
