// debug/DebugScans.cpp — Scans de memória (SCANVALUE / SCANSLOT / DIRSCAN)
//
// Extraído do AttackModule (Fase 2 do refactor reorganize-byp-modules, D4):
// são ferramentas interativas de RE, não lógica de ataque.
//
// O tag de log continua "ATTACK" para manter as linhas de log idênticas
// às de antes da extração.
#include "DebugTools.h"
#include "CommandParams.h"
#include "GameMemory.h"
#include "GameObject.h"
#include "GameAddresses.h"
#include "MessageRegistry.h"
#include "Logger.h"

#include <cstdio>
#include <cstdlib>

namespace DebugTools {

// ============================================================================
// SCANVALUE: busca um valor WORD em todos os 400 slots do ObjectManager
// Procura por attack IDs, entity indexes, ou qualquer valor relevante.
// Escaneia TODOS os slots (mesmo vazios) em busca do valor exato.
// ============================================================================
void ScanForValue(uint16_t searchValue) {
    uintptr_t arrayBase = GameMemory::GetObjectArray();
    if (!arrayBase) {
        Logger::Log("ATTACK", "SCANVALUE: ObjectArray null!");
        return;
    }

    Logger::Log("ATTACK", "==== SCANVALUE 0x%04X ====", searchValue);

    int totalFound = 0;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        int foundInSlot = 0;

        for (int off = 0; off < GameAddresses::OBJECT_SIZE; off += 2) {
            WORD val = SafeReadWord(obj + off);
            if (val == searchValue) {
                BYTE type = SafeReadByte(obj + GameObject::OFF_TYPE);
                BYTE live = SafeReadByte(obj + GameObject::OFF_ISLIVE);
                WORD compIndex = SafeReadWord(obj + GameObject::OFF_INDEX);
                char name[64] = {};
                GameMemory::GetName(i, name, sizeof(name));

                if (foundInSlot == 0) {
                    Logger::Log("ATTACK", "  [S %3d] t=%d l=%d idx=0x%04X name='%s' — FOUND 0x%04X at:",
                                i, type, live, compIndex, name, searchValue);
                }
                Logger::Log("ATTACK", "    off=0x%03X val=0x%04X", off, val);
                foundInSlot++;
                totalFound++;
                if (foundInSlot >= 10) break; // limitar por slot
            }
        }
    }

    if (totalFound == 0) {
        Logger::Log("ATTACK", "  Valor 0x%04X NÃO encontrado em nenhum slot!", searchValue);
    } else {
        Logger::Log("ATTACK", "  Total: %d ocorrências de 0x%04X", totalFound, searchValue);
    }

    // Busca adicional: procura por valores onde o high byte = high byte do searchValue
    // e o low byte está próximo (±5) do low byte do searchValue
    BYTE searchHi = (BYTE)((searchValue >> 8) & 0xFF);
    BYTE searchLo = (BYTE)(searchValue & 0xFF);
    Logger::Log("ATTACK", "  --- SCANVALUE NEAR: hi=0x%02X, lo=0x%02X (±5) ---", searchHi, searchLo);

    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        int foundInSlot = 0;

        for (int off = 0; off < GameAddresses::OBJECT_SIZE; off += 2) {
            WORD val = SafeReadWord(obj + off);
            if (val == 0xDEAD) continue;
            BYTE valHi = (BYTE)((val >> 8) & 0xFF);
            BYTE valLo = (BYTE)(val & 0xFF);
            if (valHi == searchHi && abs((int)valLo - (int)searchLo) <= 5 && val != searchValue) {
                if (foundInSlot == 0) {
                    BYTE type = SafeReadByte(obj + GameObject::OFF_TYPE);
                    BYTE live = SafeReadByte(obj + GameObject::OFF_ISLIVE);
                    WORD compIndex = SafeReadWord(obj + GameObject::OFF_INDEX);
                    char name[64] = {};
                    GameMemory::GetName(i, name, sizeof(name));
                    Logger::Log("ATTACK", "  [S %3d] t=%d l=%d idx=0x%04X name='%s' — NEAR:",
                                i, type, live, compIndex, name);
                }
                Logger::Log("ATTACK", "    off=0x%03X val=0x%04X (hi=0x%02X lo=0x%02X)",
                            off, val, valHi, valLo);
                foundInSlot++;
                if (foundInSlot >= 8) break;
            }
        }
    }

    Logger::Log("ATTACK", "==== FIM SCANVALUE ====");
}

// ============================================================================
// SCANSLOT: busca um valor WORD em UM slot específico (hex dump + busca)
// ============================================================================
void ScanSlotForValue(int slotIndex, uint16_t searchValue) {
    uintptr_t arrayBase = GameMemory::GetObjectArray();
    if (!arrayBase) {
        Logger::Log("ATTACK", "SCANSLOT: ObjectArray null!");
        return;
    }
    if (slotIndex < 0 || slotIndex >= GameAddresses::MAX_OBJECTS) {
        Logger::Log("ATTACK", "SCANSLOT: slot %d fora do range (0-%d)", slotIndex, GameAddresses::MAX_OBJECTS - 1);
        return;
    }

    uintptr_t obj = arrayBase + slotIndex * GameAddresses::OBJECT_SIZE;
    BYTE type = SafeReadByte(obj + GameObject::OFF_TYPE);
    BYTE live = SafeReadByte(obj + GameObject::OFF_ISLIVE);
    WORD compIndex = SafeReadWord(obj + GameObject::OFF_INDEX);
    char name[64] = {};
    GameMemory::GetName(slotIndex, name, sizeof(name));

    Logger::Log("ATTACK", "==== SCANSLOT slot=%d type=%d live=%d idx=0x%04X name='%s' search=0x%04X ====",
                slotIndex, type, live, compIndex, name, searchValue);

    // Mostra todos os WORDs no slot, marcando matches
    for (int off = 0; off < GameAddresses::OBJECT_SIZE; off += 2) {
        WORD val = SafeReadWord(obj + off);
        if (val == 0x0000 && off > 0x040) continue; // pular zeros depois do header
        if (val == 0xDEAD) continue;
        const char* marker = (val == searchValue) ? " <<< MATCH!" : "";
        BYTE valHi = (BYTE)((val >> 8) & 0xFF);
        BYTE valLo = (BYTE)(val & 0xFF);
        if (val != 0 || off < 0x040) {
            Logger::Log("ATTACK", "  %03X: %04X (hi=%02X lo=%02X)%s", off, val, valHi, valLo, marker);
        }
    }
    Logger::Log("ATTACK", "==== FIM SCANSLOT ====");
}

// ============================================================================
// DIRSCAN: escaneia o player local buscando o byte de direção
// Procura por bytes com valor 0-7 (direções em 8-dir) ou 0-15 (16-dir)
// em offsets candidatos no objeto do jogador local.
// ============================================================================
void DirScan() {
    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
    if (!localPlayer) {
        Logger::Log("ATTACK", "DIRSCAN: localPlayer null! Tentando ObjectArray...");
        uintptr_t arrayBase2 = GameMemory::GetObjectArray();
        if (!arrayBase2) {
            Logger::Log("ATTACK", "DIRSCAN: ObjectArray tambem null!");
            return;
        }
        // Procura jogador (type=1) no array
        for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
            uintptr_t obj = arrayBase2 + i * GameAddresses::OBJECT_SIZE;
            BYTE type = SafeReadByte(obj + GameObject::OFF_TYPE);
            BYTE live = SafeReadByte(obj + GameObject::OFF_ISLIVE);
            if (type == 1 && live) {
                localPlayer = obj;
                Logger::Log("ATTACK", "DIRSCAN: Encontrou player no slot %d (type=1, live=1)", i);
                break;
            }
        }
        if (!localPlayer) {
            Logger::Log("ATTACK", "DIRSCAN: Nenhum player encontrado!");
            return;
        }
    }

    WORD compIndex = SafeReadWord(localPlayer + GameObject::OFF_INDEX);
    char name[64] = {};
    // Busca slot index para getName
    uintptr_t arrayBase2 = GameMemory::GetObjectArray();
    int playerSlot = -1;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        if ((arrayBase2 + i * GameAddresses::OBJECT_SIZE) == localPlayer) {
            playerSlot = i;
            break;
        }
    }
    if (playerSlot >= 0) {
        GameMemory::GetName(playerSlot, name, sizeof(name));
    }

    Logger::Log("ATTACK", "==== DIRSCAN player=0x%p idx=0x%04X slot=%d name='%s' ====",
                (void*)localPlayer, compIndex, playerSlot, name);

    // Offset candidatos comuns em MU Online para direção
    static const int candidateOffsets[] = {
        0x0C, 0x0D, 0x13, 0x14, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x50, 0x54, 0x58,
        0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x71, 0x74,
        0x78, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0,
        0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC, 0xC0,
        0xC4, 0xC8, 0xCC, 0xD0, 0xD4, 0xD8, 0xDC, 0x100,
        0x104, 0x108, 0x10C, 0x110, 0x114, 0x118, 0x11C,
        0x180, 0x184, 0x188, 0x190, 0x194, 0x198, 0x19C,
        0x1A0, 0x200, 0x204, 0x208, 0x300, 0x304, 0x308,
        0x30C, 0x310, 0x314, 0x318, 0x320, 0x324, 0x328,
        0x400, 0x404, 0x408, 0x40C, 0x410, 0x414, 0x418,
    };

    Logger::Log("ATTACK", "--- Offsets candidatos (bytes com valor 0-15) ---");
    for (int off : candidateOffsets) {
        BYTE b = SafeReadByte(localPlayer + off);
        if (b <= 15) {
            Logger::Log("ATTACK", "  off=0x%03X byte=0x%02X (%d) *** CANDIDATO ***", off, b, b);
        } else {
            Logger::Log("ATTACK", "  off=0x%03X byte=0x%02X (%d)", off, b, b);
        }
    }

    // Escaneamento completo: todos os bytes com valor 0-7 no range 0x00-0x440
    Logger::Log("ATTACK", "--- Escaneamento completo: bytes com valor 0-7 no range 0x00-0x440 ---");
    for (int off = 0; off < 0x440; off++) {
        BYTE b = SafeReadByte(localPlayer + off);
        if (b <= 7) {
            Logger::Log("ATTACK", "  off=0x%03X byte=0x%02X (%d)", off, b, b);
        }
    }

    // Escaneamento para bytes com valor 8-15 (16-dir)
    Logger::Log("ATTACK", "--- Escaneamento completo: bytes com valor 8-15 no range 0x00-0x440 ---");
    for (int off = 0; off < 0x440; off++) {
        BYTE b = SafeReadByte(localPlayer + off);
        if (b >= 8 && b <= 15) {
            Logger::Log("ATTACK", "  off=0x%03X byte=0x%02X (%d)", off, b, b);
        }
    }

    Logger::Log("ATTACK", "==== FIM DIRSCAN ====");
}

// ============================================================================
// Handlers de mensagens tipadas (registrados via REGISTER_MESSAGE)
// ============================================================================

static void HandleScanValue(const char* payload) {
    uint16_t value = 0;
    if (!CommandParams::ParseHexParam(payload, "value", &value) && payload && *payload) {
        unsigned int raw = 0;
        if (sscanf_s(payload, "%x", &raw) == 1) value = (uint16_t)raw;  // payload cru = hex
    }
    ScanForValue(value);
}
REGISTER_MESSAGE(ATTACK_SCANVALUE, HandleScanValue);

static void HandleScanSlot(const char* payload) {
    int slot = 0;
    uint16_t value = 0;
    CommandParams::ParseIntParam(payload, "slot", &slot);
    CommandParams::ParseHexParam(payload, "value", &value);
    ScanSlotForValue(slot, value);
}
REGISTER_MESSAGE(ATTACK_SCANSLOT, HandleScanSlot);

static void HandleDirScan(const char* payload) {
    (void)payload;
    DirScan();
}
REGISTER_MESSAGE(ATTACK_DIRSCAN, HandleDirScan);

} // namespace DebugTools
