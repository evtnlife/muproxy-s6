// debug/DebugTools.cpp — Diagnóstico de memória: dump do ObjectManager + hexdump
//
// Extraído do AttackModule (Fase 2 do refactor reorganize-byp-modules, D4):
// são ferramentas interativas de RE, não lógica de ataque.
//
// O tag de log continua "ATTACK" para manter as linhas de log idênticas
// às de antes da extração.
#include "DebugTools.h"
#include "CommandParams.h"
#include "GameMemory/Object.h"
#include "GameMemory/Player.h"
#include "GameObject.h"
#include "GameAddresses.h"
#include "MessageRegistry.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace DebugTools {

// ---- Constante de conversão de coordenadas ----
// No MU Online, as posições X/Y na memória estão em centésimos de tile
// (ex: 19250, 8150). Para exibir a distância em tiles, divide por 100.
static constexpr float TILE_SIZE = 100.0f;

// ============================================================================
// Leituras crash-safe (proteção contra access violation)
// ============================================================================
DWORD SafeReadDword(uintptr_t addr) {
    __try { return *(DWORD*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xDEADDEAD; }
}
WORD SafeReadWord(uintptr_t addr) {
    __try { return *(WORD*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xDEAD; }
}
BYTE SafeReadByte(uintptr_t addr) {
    __try { return *(BYTE*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xFF; }
}
float SafeReadFloat(uintptr_t addr) {
    __try { return *(float*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -9999.0f; }
}

// ============================================================================
// DIAGNÓSTICO: despeja o ObjectManager completo com viewport scan
// ============================================================================
void DumpObjectManager() {
    uintptr_t arrayBase = GameMemory::Object::Array();
    if (!arrayBase) {
        Logger::Log("ATTACK", "DUMP: ObjectArray null!");
        return;
    }

    uintptr_t localPlayer = GameMemory::Player::Object();
    float lx = localPlayer ? SafeReadFloat(localPlayer + GameObject::OFF_X) : 0.0f;
    float ly = localPlayer ? SafeReadFloat(localPlayer + GameObject::OFF_Y) : 0.0f;

    Logger::Log("ATTACK", "==== DUMP FULL ObjectManager local=(%.1f,%.1f) ====", lx, ly);

    int count = 0;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;

        BYTE type = SafeReadByte(obj + GameObject::OFF_TYPE);
        BYTE live = SafeReadByte(obj + GameObject::OFF_ISLIVE);
        if (type == 0 || !live) continue;

        WORD compIndex = SafeReadWord(obj + GameObject::OFF_INDEX);
        float x = SafeReadFloat(obj + GameObject::OFF_X);
        float y = SafeReadFloat(obj + GameObject::OFF_Y);
        char name[64] = {};
        GameMemory::Object::Name(i, name, sizeof(name));

        float dx = x - lx, dy = y - ly;
        float dist = sqrtf(dx * dx + dy * dy);
        float distTiles = dist / TILE_SIZE;

        BYTE compHi = (BYTE)((compIndex >> 8) & 0xFF);
        BYTE compLo = (BYTE)(compIndex & 0xFF);
        BYTE slotByte = (BYTE)(i & 0xFF);

        // ===== Campos-chave com SafeRead (crash-safe) =====
        DWORD vtblPtr = SafeReadDword(obj + 0x000);
        DWORD dw074   = SafeReadDword(obj + 0x074);
        DWORD dw078   = SafeReadDword(obj + 0x078);
        DWORD dw07C   = SafeReadDword(obj + 0x07C);
        DWORD dw080   = SafeReadDword(obj + 0x080);
        DWORD dw084   = SafeReadDword(obj + 0x084);
        DWORD dw29C   = SafeReadDword(obj + 0x29C);
        DWORD dw2A0   = SafeReadDword(obj + 0x2A0);
        DWORD dw2A4   = SafeReadDword(obj + 0x2A4);
        DWORD dw2A8   = SafeReadDword(obj + 0x2A8);
        DWORD dw30C   = SafeReadDword(obj + 0x30C);
        DWORD dw320   = SafeReadDword(obj + 0x320);

        WORD w070 = SafeReadWord(obj + 0x070);
        WORD w072 = SafeReadWord(obj + 0x072);
        WORD w076 = SafeReadWord(obj + 0x076);
        WORD w078w = SafeReadWord(obj + 0x078);
        WORD w07A = SafeReadWord(obj + 0x07A);
        WORD w07Cw = SafeReadWord(obj + 0x07C);
        WORD w07E = SafeReadWord(obj + 0x07E);  // OFF_INDEX
        WORD w080w = SafeReadWord(obj + 0x080);
        WORD w082 = SafeReadWord(obj + 0x082);
        WORD w084w = SafeReadWord(obj + 0x084);

        BYTE dirByte = SafeReadByte(obj + 0x070);  // byte de direção (ObjectPreview)

        Logger::Log("ATTACK", "  [S %3d] t=%d l=%d idx=0x%04X[hi=%02X lo=%02X] slot=0x%02X(%d) pos=(%.1f,%.1f) d=%.0f(%.1ft) dir=%d(0x%02X) '%s'",
                    i, type, live, compIndex, compHi, compLo, slotByte, slotByte, x, y, dist, distTiles, dirByte, dirByte, name);
        Logger::Log("ATTACK", "    vtbl=0x%08X dw074=0x%08X dw078=0x%08X dw07C=0x%08X dw080=0x%08X dw084=0x%08X",
                    vtblPtr, dw074, dw078, dw07C, dw080, dw084);
        Logger::Log("ATTACK", "    w070=%04X w072=%04X w076=%04X w078=%04X w07A=%04X w07C=%04X w07E=%04X w080=%04X w082=%04X w084=%04X",
                    w070, w072, w076, w078w, w07A, w07Cw, w07E, w080w, w082, w084w);
        Logger::Log("ATTACK", "    vp: dw29C=0x%08X dw2A0=0x%08X dw2A4=0x%08X dw2A8=0x%08X",
                    dw29C, dw2A0, dw2A4, dw2A8);
        Logger::Log("ATTACK", "    live=0x%08X type=0x%08X",
                    dw30C, dw320);

        // ===== ATTACK ID SCAN: procura WORDs com byte alto = slotByte =====
        int foundCount = 0;
        for (int off = 0; off < GameAddresses::OBJECT_SIZE && foundCount < 8; off += 2) {
            WORD val = SafeReadWord(obj + off);
            if (val == 0xDEAD) continue;
            BYTE valHi = (BYTE)((val >> 8) & 0xFF);
            if (valHi == slotByte && val != 0 && val != compIndex && off != GameObject::OFF_INDEX) {
                if (foundCount == 0) {
                    Logger::Log("ATTACK", "    ATTACK_ID_CANDIDATES (slotByte=0x%02X):", slotByte);
                }
                Logger::Log("ATTACK", "      off=0x%03X val=0x%04X (hi=0x%02X lo=0x%02X)", off, val, valHi, (BYTE)(val & 0xFF));
                foundCount++;
            }
        }

        count++;
    }
    Logger::Log("ATTACK", "==== FIM DUMP %d objs vivos ====", count);
}

// ============================================================================
// HEX DUMP: despeja o slot inteiro (0x598 bytes) com anotações
// ============================================================================
void HexDumpSlot(int slotIndex) {
    uintptr_t arrayBase = GameMemory::Object::Array();
    if (!arrayBase) {
        Logger::Log("ATTACK", "HEXDUMP: ObjectArray null!");
        return;
    }
    if (slotIndex < 0 || slotIndex >= GameAddresses::MAX_OBJECTS) {
        Logger::Log("ATTACK", "HEXDUMP: slot %d fora do range (0-%d)", slotIndex, GameAddresses::MAX_OBJECTS - 1);
        return;
    }

    uintptr_t obj = arrayBase + slotIndex * GameAddresses::OBJECT_SIZE;
    BYTE type = *(BYTE*)(obj + GameObject::OFF_TYPE);
    BYTE live = *(BYTE*)(obj + GameObject::OFF_ISLIVE);
    WORD compIndex = *(WORD*)(obj + GameObject::OFF_INDEX);
    BYTE slotByte = (BYTE)(slotIndex & 0xFF);
    char name[64];
    GameMemory::Object::Name(slotIndex, name, sizeof(name));

    Logger::Log("ATTACK", "==== HEXDUMP slot=%d type=%d live=%d idx=0x%04X slotByte=0x%02X(%d) name='%s' ====",
                slotIndex, type, live, compIndex, slotByte, slotByte, name);

    // Nomes de offsets conhecidos para anotação
    struct OffsetName { int offset; const char* name; };
    const OffsetName knownOffsets[] = {
        {0x000, "vtbl"}, {0x038, "NAME"}, {0x07E, "INDEX"},
        {0x19C, "LEVEL"}, {0x208, "LEVEL2"}, {0x29C, "INTPTR"},
        {0x30C, "ISLIVE"}, {0x320, "TYPE"}, {0x338, "FLOAT1"},
        {0x368, "SET"}, {0x404, "X"}, {0x408, "Y"},
        {0x40C, "Z"}, {0x410, "X_SET"}, {0x414, "Y_SET"}, {0x418, "Z_SET"},
    };

    // Dump em blocos de 16 bytes (slot inteiro = 0x598 bytes = 89 rows)
    for (int row = 0; row < GameAddresses::OBJECT_SIZE; row += 16) {
        BYTE b[16];
        memcpy(b, (BYTE*)(obj + row), 16);

        // ASCII representation
        char ascii[17];
        for (int j = 0; j < 16; j++) {
            ascii[j] = (b[j] >= 0x20 && b[j] <= 0x7E) ? (char)b[j] : '.';
        }
        ascii[16] = '\0';

        // Check if any known offset falls in this row
        char annotation[128] = "";
        for (const auto& kn : knownOffsets) {
            if (kn.offset >= row && kn.offset < row + 16) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), " <%s@0x%03X>", kn.name, kn.offset);
                strcat_s(annotation, sizeof(annotation), tmp);
            }
        }

        Logger::Log("ATTACK", "  %03X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X  %s%s",
                    row, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                    b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15],
                    ascii, annotation);
    }

    // ATTACK ID SCAN: procura por WORDs com byte alto = slotByte em todo o slot
    Logger::Log("ATTACK", "  --- ATTACK ID SCAN (slotByte=0x%02X) ---", slotByte);
    for (int off = 0; off < GameAddresses::OBJECT_SIZE; off += 2) {
        WORD val = *(WORD*)(obj + off);
        BYTE valHi = (BYTE)((val >> 8) & 0xFF);
        if (valHi == slotByte && val != 0 && val != compIndex) {
            Logger::Log("ATTACK", "    offset=0x%03X value=0x%04X (hi=0x%02X lo=0x%02X) MATCH slotByte!",
                        off, val, valHi, (BYTE)(val & 0xFF));
        }
    }

    Logger::Log("ATTACK", "==== FIM HEXDUMP slot=%d ====", slotIndex);
}

// ============================================================================
// Handlers de mensagens tipadas (registrados via REGISTER_MESSAGE)
// ============================================================================

static void HandleDump(const char* payload) {
    (void)payload;
    DumpObjectManager();
}
REGISTER_MESSAGE(ATTACK_DUMP, HandleDump);

static void HandleHexDump(const char* payload) {
    int slot = 0;
    if (!CommandParams::ParseIntParam(payload, "slot", &slot) && payload && *payload) {
        slot = atoi(payload);  // payload cru = número do slot
    }
    HexDumpSlot(slot);
}
REGISTER_MESSAGE(ATTACK_HEXDUMP, HandleHexDump);

} // namespace DebugTools
