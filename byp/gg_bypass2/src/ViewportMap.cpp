// ViewportMap.cpp — Mapa dinâmico viewport index -> attack index
#include "include/ViewportMap.h"
#include "include/Logger.h"

#include <map>
#include <cstring>

namespace ViewportMap {

// Mapa: viewportIndex (2 bytes) -> attackIndex (2 bytes)
static std::map<uint16_t, uint16_t> g_Map;

// Extrai o entity index real (sem flags) de 2 bytes.
// MuEmu: actualIndex = ((hi & 0x3F) << 8) | lo
static int ExtractEntityIndex(uint8_t hi, uint8_t lo) {
    return ((hi & 0x3F) << 8) | lo;
}

// Extrai a monster class (sem flags) de 2 bytes.
// MuEmu: actualClass = ((hi & 0x0F) << 8) | lo
static int ExtractMonsterClass(uint8_t hi, uint8_t lo) {
    return ((hi & 0x0F) << 8) | lo;
}

void OnViewportPacket(const uint8_t* data, int len) {
    if (!data || len < 8) return;

    // Header C2: data[0]=0xC2, data[1]=sizeHi, data[2]=sizeLo, data[3]=0x13, data[4]=count
    int hdrSize = (data[0] == 0xC2 || data[0] == 0xC4) ? 3 : 2;
    if (data[hdrSize] != 0x13) return;

    int count = data[hdrSize + 1];
    int i = hdrSize + 2; // começa após count

    int recordsParsed = 0;
    while (i + 9 <= len && recordsParsed < count) {
        uint8_t indexHi = data[i];
        uint8_t indexLo = data[i + 1];
        uint8_t typeHi  = data[i + 2];
        uint8_t typeLo  = data[i + 3];

        int x = data[i + 4];
        int y = data[i + 5];
        // tx = data[i+6], ty = data[i+7], dirAndPkLevel = data[i+8]
        uint8_t effectCount = data[i + 9];

        // Registro base = 10 bytes. Campos extras (GAMESERVER_UPDATE>=701):
        //   attribute(1) + level(2) + MaxHP(4) + CurHP(4) = 11 bytes, se effectCount > 0
        int extraSize = 0;
        if (i + 10 + 11 <= len && effectCount > 0) {
            extraSize = 11;
        }
        int recordSize = 10 + extraSize;
        if (effectCount > 0) {
            int effectsSize = effectCount * 2;
            if (i + recordSize + effectsSize > len)
                effectsSize = len - (i + recordSize);
            recordSize += effectsSize;
        }
        if (i + recordSize > len) {
            recordSize = len - i;
            if (recordSize < 10) break;
        }

        int viewportIndex = ExtractEntityIndex(indexHi, indexLo);
        int monsterClass = ExtractMonsterClass(typeHi, typeLo);

        if (viewportIndex != 0) {
            // attackIndex = (viewport_idxLo << 8) | monsterClass_lo
            uint16_t attackIndex = (uint16_t)((indexLo << 8) | typeLo);

            g_Map[(uint16_t)viewportIndex] = attackIndex;

            Logger::Log("VIEWPORT", "0x13 idx=0x%04X (lo=0x%02X) class=0x%04X (lo=0x%02X) pos=(%d,%d) attackIdx=0x%04X",
                        viewportIndex, indexLo, monsterClass, typeLo, x, y, attackIndex);
        }

        i += recordSize;
        recordsParsed++;
    }
}

uint16_t GetAttackIndex(uint16_t viewportIndex) {
    auto it = g_Map.find(viewportIndex);
    if (it != g_Map.end()) return it->second;
    return 0;
}

void Clear() {
    g_Map.clear();
}

} // namespace ViewportMap
