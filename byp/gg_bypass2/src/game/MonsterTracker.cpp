// MonsterTracker.cpp — Escaneia array de objetos e envia dados via TCP
#include "MonsterTracker.h"
#include "GameMemory.h"
#include "GameAddresses.h"
#include "GameObject.h"
#include "ObjType.h"
#include "CommandChannel.h"
#include "Logger.h"
#include "PacketHandler.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace MonsterTracker {

static HANDLE g_hThread = nullptr;
static volatile LONG g_Running = 0;
static volatile LONG g_PeriodicEnabled = 0;

// ---- Configurações dinâmicas ----
static TrackerConfig g_Config = {
    500,    // scanIntervalMs
    0.0f,   // maxRange (0 = sem limite)
    0x7     // filterMask (bits: 1=player, 2=monster, 4=npc)
};

TrackerConfig& GetConfig() { return g_Config; }

// ---- Utilitários de parse key=value ----
static bool ParseIntParam(const char* s, const char* key, int* outVal) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(s, search);
    if (!p) return false;
    p += strlen(search);
    *outVal = atoi(p);
    return true;
}

static bool ParseFloatParam(const char* s, const char* key, float* outVal) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(s, search);
    if (!p) return false;
    p += strlen(search);
    *outVal = (float)atof(p);
    return true;
}

// Calcula distância euclidiana entre local player e coordenadas
static float DistanceToLocal(float tx, float ty) {
    uintptr_t localPlayer = GameMemory::Player::Object();
    if (!localPlayer) return 0.0f;

    float lx = *(float*)(localPlayer + GameObject::OFF_X);
    float ly = *(float*)(localPlayer + GameObject::OFF_Y);
    float dx = tx - lx;
    float dy = ty - ly;
    return sqrtf(dx * dx + dy * dy);
}

// Verifica se um tipo de objeto passa no filtro
static bool MatchesFilter(uint8_t objType, int filterMask) {
    // Bit 1 = player (OBJ_PLAYER=1), bit 2 = monster (OBJ_MONSTER=2), bit 4 = npc (OBJ_NPC=4)
    if (objType == OBJ_PLAYER) return (filterMask & 0x02) != 0;
    if (objType == OBJ_MONSTER) return (filterMask & 0x04) != 0;
    if (objType == OBJ_NPC) return (filterMask & 0x08) != 0;
    return (filterMask & 0x01) != 0;  // empty/outros
}

// Envia lista completa de objetos como múltiplas linhas TCP
// filterMask: bitmask de tipos para incluir (0 = todos)
// maxRange: distância máxima do local player (0 = sem limite)
void SendFullListFiltered(int filterMask, float maxRange) {
    uintptr_t arrayBase = GameMemory::Object::Array();
    if (!arrayBase) {
        Logger::Log("MONSTER", "Array de objetos nao disponivel");
        return;
    }

    int count = 0;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        uint8_t type = *(uint8_t*)(obj + GameObject::OFF_TYPE);
        if (type == OBJ_EMPTY) continue;

        // Filtro por tipo
        if (filterMask != 0 && !MatchesFilter(type, filterMask))
            continue;

        uint8_t isLive = *(uint8_t*)(obj + GameObject::OFF_ISLIVE);
        if (!isLive) continue;

        char name[32] = {};
        const char* src = (const char*)(obj + GameObject::OFF_NAME);
        for (int n = 0; n < 31 && src[n]; ++n) name[n] = src[n];

        uint16_t compositeIndex = *(uint16_t*)(obj + GameObject::OFF_INDEX);
        float x = *(float*)(obj + GameObject::OFF_X);
        float y = *(float*)(obj + GameObject::OFF_Y);
        float z = *(float*)(obj + GameObject::OFF_Z);
        short level = 0; // ObjectPreview nao tem level (vem por pacote)

        // Calcula distância do local player
        float dist = DistanceToLocal(x, y);

        // Filtro por distância
        if (maxRange > 0.0f && dist > maxRange)
            continue;

        // Formato tipado: "ENTITY|slot|compositeIdx|type|level|x|y|z|dist|name"
        CommandChannel::SendFormat("ENTITY|%d|%d|%d|%d|%.1f|%.1f|%.1f|%.1f|%s",
            i, (int)compositeIndex, (int)type, (int)level, x, y, z, dist, name);
        count++;
    }

    // Envia marcador de fim
    CommandChannel::SendFormat("ENTITY_BATCH_END|%d", count);
    Logger::Log("MONSTER", "Lista enviada: %d objetos (filter=0x%X, range=%.1f)", count, filterMask, maxRange);
}

void SendFullList() {
    SendFullListFiltered(g_Config.filterMask, g_Config.maxRange);
}

void SendDeltaList() {
    // TODO: implementar delta (só envia objetos que mudaram desde último scan)
    // Por enquanto, envia lista completa com filtros atuais
    SendFullList();
}

static DWORD WINAPI ScanThread(LPVOID) {
    Logger::Log("MONSTER", "Thread de scan periódico iniciada (interval=%dms, filter=0x%X, range=%.1f)",
                g_Config.scanIntervalMs, g_Config.filterMask, g_Config.maxRange);
    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        if (InterlockedCompareExchange(&g_PeriodicEnabled, 0, 0) == 1) {
            SendDeltaList();
        }
        Sleep(g_Config.scanIntervalMs);
    }
    Logger::Log("MONSTER", "Thread de scan periódico encerrada");
    return 0;
}

void OnCommand(const char* opcode, const char* subCmd,
               const uint8_t* data, int dataLen) {
    (void)opcode;

    if (strcmp(subCmd, "LIST") == 0 || strcmp(subCmd, "SINGLE") == 0) {
        // Permite passar filter e range no data
        int filter = g_Config.filterMask;
        float range = g_Config.maxRange;

        if (dataLen > 0) {
            char paramBuf[256];
            if (dataLen < (int)sizeof(paramBuf)) {
                memcpy(paramBuf, data, dataLen);
                paramBuf[dataLen] = '\0';
                int ival;
                float fval;
                ParseIntParam(paramBuf, "filter", &ival);
                if (ival > 0) filter = ival;
                ParseFloatParam(paramBuf, "range", &fval);
                if (fval > 0.0f) range = fval;
            }
        }
        SendFullListFiltered(filter, range);
    }
    else if (strcmp(subCmd, "START") == 0) {
        // CMD MONSTER START [interval=<ms>] [filter=<mask>] [range=<float>]
        if (dataLen > 0) {
            char paramBuf[256];
            if (dataLen < (int)sizeof(paramBuf)) {
                memcpy(paramBuf, data, dataLen);
                paramBuf[dataLen] = '\0';
                int ival;
                if (ParseIntParam(paramBuf, "interval", &ival) && ival > 0)
                    g_Config.scanIntervalMs = ival;
                if (ParseIntParam(paramBuf, "filter", &ival) && ival > 0)
                    g_Config.filterMask = ival;
                ParseFloatParam(paramBuf, "range", &g_Config.maxRange);
            }
        }
        InterlockedExchange(&g_PeriodicEnabled, 1);
        Logger::Log("MONSTER", "Scan periódico iniciado (interval=%dms, filter=0x%X, range=%.1f)",
                    g_Config.scanIntervalMs, g_Config.filterMask, g_Config.maxRange);
    }
    else if (strcmp(subCmd, "STOP") == 0) {
        InterlockedExchange(&g_PeriodicEnabled, 0);
        Logger::Log("MONSTER", "Scan periódico parado");
    }
    else if (strcmp(subCmd, "CONFIG") == 0) {
        // CMD MONSTER CONFIG interval=<ms> filter=<mask> range=<float>
        if (dataLen > 0) {
            char paramBuf[256];
            if (dataLen < (int)sizeof(paramBuf)) {
                memcpy(paramBuf, data, dataLen);
                paramBuf[dataLen] = '\0';
                int ival;
                if (ParseIntParam(paramBuf, "interval", &ival) && ival > 0)
                    g_Config.scanIntervalMs = ival;
                if (ParseIntParam(paramBuf, "filter", &ival) && ival > 0)
                    g_Config.filterMask = ival;
                ParseFloatParam(paramBuf, "range", &g_Config.maxRange);
            }
        }
        Logger::Log("MONSTER", "CONFIG: interval=%dms filter=0x%X range=%.1f",
                    g_Config.scanIntervalMs, g_Config.filterMask, g_Config.maxRange);
    }
    else {
        Logger::Log("MONSTER", "Comando desconhecido: %s", subCmd);
    }
}

bool Initialize() {
    PacketHandler::RegisterHandler("MONSTER", OnCommand);

    g_Running = 1;
    g_hThread = CreateThread(NULL, 0, ScanThread, NULL, 0, NULL);
    if (!g_hThread) {
        Logger::Log("MONSTER", "FALHA ao criar thread de scan");
        g_Running = 0;
        return false;
    }

    Logger::Log("MONSTER", "Inicializado (thread=%p, interval=%dms, filter=0x%X, range=%.1f)",
                g_hThread, g_Config.scanIntervalMs, g_Config.filterMask, g_Config.maxRange);
    return true;
}

void Shutdown() {
    InterlockedExchange(&g_PeriodicEnabled, 0);
    InterlockedExchange(&g_Running, 0);
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 2000);
        CloseHandle(g_hThread);
        g_hThread = nullptr;
    }
    Logger::Log("MONSTER", "Desligado");
}

} // namespace MonsterTracker