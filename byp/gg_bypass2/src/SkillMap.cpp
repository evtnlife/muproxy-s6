// SkillMap.cpp — Mapa dinâmico SkillNumber -> SkillIndex
#include "include/SkillMap.h"
#include "include/Logger.h"

#include <map>

namespace SkillMap {

// Mapa bidirecional: skillIndex -> skillNumber e skillNumber -> skillIndex
static std::map<uint8_t, uint16_t> g_IndexToNumber;
static std::map<uint16_t, uint8_t> g_NumberToIndex;

// Registra uma skill no mapa. skillIndex é o byte usado no pacote 0x19.
static void RegisterSkill(uint8_t skillIndex, uint16_t skillNumber, uint8_t skillLevel) {
    if (skillNumber == 0) return;
    g_IndexToNumber[skillIndex] = skillNumber;
    g_NumberToIndex[skillNumber] = skillIndex;
    Logger::Log("SKILLMAP", "ADD: index=0x%02X number=0x%04X level=%d",
                skillIndex, skillNumber, skillLevel);
}

void OnSkillListPacket(const uint8_t* data, int len) {
    if (!data || len < 6) return;

    int hdrSize = (data[0] == 0xC2 || data[0] == 0xC4) ? 3 : 2;
    if (hdrSize + 2 > len) return;
    if (data[hdrSize] != 0xF3) return;

    uint8_t subcode = data[hdrSize + 1];

    // ---- F3 11 (SkillListUpdate, OpenMU) — o BlackRock ENVIA (confirmado no
    // capture: C1 0A F3 11 FE 00 08 13 00 00).
    // Layout: [count] [ListType] [SkillEntry*4]...
    // SkillEntry (4B): SkillIndex(1), SkillNumber(2 LE), SkillLevel(1).
    // entryStart DEVE pular o byte ListType (hdrSize+4), senão lê ListType
    // como SkillIndex e o SkillNumber sai deslocado (0x1308 em vez de 0x0013).
    if (subcode == 0x11) {
        uint8_t count = data[hdrSize + 2];
        // ListType: 0 = limpar lista e adicionar, 2 = remover, senão = adicionar.
        uint8_t listType = data[hdrSize + 3];
        int entryStart = hdrSize + 4;

        if (count == 0xFE || count == 0xFF) {
            if (entryStart + 4 > len) return;
            uint8_t skillIndex = data[entryStart];
            uint16_t skillNumber = (uint16_t)(data[entryStart + 1] | (data[entryStart + 2] << 8));
            uint8_t skillLevel = data[entryStart + 3];

            if (count == 0xFE || listType != 2) {
                RegisterSkill(skillIndex, skillNumber, skillLevel);
            } else {
                g_IndexToNumber.erase(skillIndex);
                g_NumberToIndex.erase(skillNumber);
                Logger::Log("SKILLMAP", "F3 11 REMOVE: index=0x%02X number=0x%04X",
                            skillIndex, skillNumber);
            }
            return;
        }

        for (int i = 0; i < count; ++i) {
            int off = entryStart + i * 4;
            if (off + 4 > len) break;
            uint8_t skillIndex = data[off];
            uint16_t skillNumber = (uint16_t)(data[off + 1] | (data[off + 2] << 8));
            uint8_t skillLevel = data[off + 3];
            if (skillIndex == 0 && skillNumber == 0) continue;
            RegisterSkill(skillIndex, skillNumber, skillLevel);
        }
        return;
    }

    // ---- F3 E2 (SkillListMulti) — o que o BlackRock realmente envia. ----
    // C2 [sizeHi sizeLo] F3 E2 [count] [entry*24]
    if (subcode == 0xE2) {
        uint8_t count = data[hdrSize + 2];
        int entryStart = hdrSize + 3;
        const int ENTRY_SIZE = 24;

        for (int i = 0; i < count; ++i) {
            int off = entryStart + i * ENTRY_SIZE;
            if (off + ENTRY_SIZE > len) break;

            uint16_t skillNumber = (uint16_t)(data[off] | (data[off + 1] << 8));
            uint16_t skillLevel  = (uint16_t)(data[off + 20] | (data[off + 21] << 8));

            if (skillNumber == 0) continue;

            // SkillIndex = byte baixo do skillNumber (Inferno 0x01E7 -> 0xE7).
            // Confirmado pelo pacote real C1 07 19 E7 EE CF 12.
            uint8_t skillIndex = (uint8_t)(skillNumber & 0xFF);

            RegisterSkill(skillIndex, skillNumber, (uint8_t)skillLevel);
        }
        return;
    }
}

uint8_t GetSkillIndex(uint16_t skillNumber) {
    auto it = g_NumberToIndex.find(skillNumber);
    if (it != g_NumberToIndex.end()) return it->second;
    // Fallback: byte baixo do skillNumber (Inferno 0x01E7 -> 0xE7)
    return (uint8_t)(skillNumber & 0xFF);
}

uint16_t GetSkillNumber(uint8_t skillIndex) {
    auto it = g_IndexToNumber.find(skillIndex);
    if (it != g_IndexToNumber.end()) return it->second;
    return 0;
}

void Clear() {
    g_IndexToNumber.clear();
    g_NumberToIndex.clear();
}

} // namespace SkillMap
