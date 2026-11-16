// AttackModule.cpp — Monta e envia pacotes de ataque (normal + skill)
#include "AttackModule.h"
#include "MuOpcodes.h"
#include "GameMemory.h"
#include "GameObject.h"
#include "ObjType.h"
#include "GameAddresses.h"
#include "CommandChannel.h"
#include "PacketHandler.h"
#include "MessageRegistry.h"
#include "PacketSniffer.h"
#include "PacketInjector.h"
#include "SkillMap.h"
#include "Logger.h"
#include "CommandParams.h"
#include "DebugTools.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <set>

// Lock para counter anti-replay (0x8793700) — acessado de multiplas threads
static CRITICAL_SECTION g_smCounterLock;

namespace AttackModule {

// ---- Constante de conversão de coordenadas ----
// No MU Online, as posições X/Y na memória estão em centésimos de tile (ex: 19250, 8150).
// O range vem em tiles (ex: range=8), então dividimos a distância por 100 para comparar.
static constexpr float TILE_SIZE = 100.0f;

// ---- Estado global ----
static HANDLE g_hThread = nullptr;
static volatile LONG g_Running = 0;
static volatile LONG g_Attacking = 0;

// ---- Monster Map: entityIndex -> ativo (protegido por CriticalSection) ----
static std::map<uint16_t, bool> g_MonsterMap;
static CRITICAL_SECTION g_MapLock;
static HANDLE g_hNewMonsterEvent = nullptr;  // sinalizado quando novo monstro aparece
static HANDLE g_hWatcherThread = nullptr;
static HANDLE g_hDispatcherThread = nullptr;

// ---- Configurações dinâmicas ----
static AttackConfig g_Config = {
    100,    // defaultDelayMs
    0.0f,   // defaultRange (0 = sem limite)
    0,      // defaultType (0 = normal)
    0x003C, // defaultSkillId (60 = SKILL_FORCE do DL — número real da skill)
    1       // defaultHitCount
};

// ---- Parâmetros da sessão de ataque ativa ----
static int     g_SessionDelayMs = 800;
static float   g_SessionRange   = 0.0f;
static uint8_t g_SessionType    = 0;      // 0=normal, 1=skill
static uint16_t g_SessionSkillId = 0;
static int     g_SessionHitCount = 1;     // hits por mob por ciclo

AttackConfig& GetConfig() { return g_Config; }

// ---- Distância entre local player e o slot do alvo ----
static float DistanceToSlot(int slotIndex) {
    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
    if (!localPlayer) return 9999.0f;

    float lx = *(float*)(localPlayer + GameObject::OFF_X);
    float ly = *(float*)(localPlayer + GameObject::OFF_Y);

    uintptr_t obj = GameMemory::GetObjectByIndex(slotIndex);
    if (!obj) return 9999.0f;

    float tx = *(float*)(obj + GameObject::OFF_X);
    float ty = *(float*)(obj + GameObject::OFF_Y);
    float dx = tx - lx;
    float dy = ty - ly;
    return sqrtf(dx * dx + dy * dy);
}

// ---- Calcula byte de direção via atan2 ----
// O jogo usa atan2 customizado que retorna graus com 0°=Norte.
// 8 direções (0-7). VALIDADO (RE 2 camadas): o byte D3 do pacote 0x11 é 0..7
// PURO (disasm: [ebp-0x14] & 0x80000007), SEM máscara 0xC0.
// (O "0xC5 (SW)" do comentário antigo era a forma XorData do D3=0x04 real.)
// Fórmula: dir = floor((atan2(dy,dx) + PI/2) / (2*PI) * 8) % 8
// Resultado: N=0, NE=1, E=2, SE=3, S=4, SW=5, W=6, NW=7
static uint8_t ComputeDirection(uint16_t targetEntityIndex) {
    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
    if (!localPlayer) return 0;

    uintptr_t targetObj = GameMemory::FindObjectByEntityIndex(targetEntityIndex);
    if (!targetObj) return 0;

    float playerX = *(float*)(localPlayer + GameObject::OFF_X);
    float playerY = *(float*)(localPlayer + GameObject::OFF_Y);
    float targetX = *(float*)(targetObj + GameObject::OFF_X);
    float targetY = *(float*)(targetObj + GameObject::OFF_Y);

    if (playerX == 0.0f && playerY == 0.0f) return 0;
    if (targetX == 0.0f && targetY == 0.0f) return 0;

    float dx = targetX - playerX;
    float dy = targetY - playerY;
    if (dx == 0.0f && dy == 0.0f) return 0;

    // Fórmula MU Online (Y cresce para baixo na tela):
    // dir = floor((atan2(dy, dx) + PI/2) / (2*PI) * 8) % 8
    // Resultado: N=0, NE=1, E=2, SE=3, S=4, SW=5, W=6, NW=7
    const float PI = 3.14159265f;
    float angle = atan2f(dy, dx);
    float dir = floorf((angle + PI * 0.5f) / (2.0f * PI) * 8.0f);
    int dirInt = (int)dir % 8;
    if (dirInt < 0) dirInt += 8;

    // VALIDADO: D3 é 0..7 puro (sem 0xC0). O pacote real tem D3=0x04 (S).
    uint8_t dirByte = (uint8_t)dirInt;

    return dirByte;
}

// ---- Monta pacote de ataque normal 0x11 (PLAINTEXT REAL, pré-criptografia) ----
// Formato VALIDADO (RE 2 camadas, 24 capturas nativas do byp_packets.log):
//   C1 07 11 <targetHi> <targetLo> 78 <dir 0..7>
//   - target = EntityIndex PURO (SEM XOR 0xEFF5!), big-endian.
//     VALIDADO em campo: o builder nativo coloca o índice cru em D0D1
//     (monstro 0x001A -> D0D1 = 00 1A em TODAS as capturas; D2=0x78 e
//     D3 em 0..7 nos 4 alvos diferentes 0x001A/0x001B/0x0025/0x005A).
//     O skill 0x19 também usa índice puro (decodificação da captura: index=0x0277).
//   - D2 = 0x78 CONSTANTE (disasm builder 0x5A1701: push 0x78; 0x78 em todas as 24 capturas)
//   - D3 = direção 0..7 (disasm: [ebp-0x14] & 0x80000007)
// Este buffer é o PLAINTEXT REAL. A criptografia (XorData [3,7) + Layer-1)
// é aplicada em PacketInjector::Inject (src/packets/PacketInjector.cpp).
static int BuildAttackPacket(uint8_t* outBuf, uint16_t entityIndex) {
    outBuf[0] = MuOpcodes::HEADER_C1;           // 0xC1
    outBuf[1] = MuOpcodes::SIZE_ATTACK_NORMAL;  // 0x07
    outBuf[2] = MuOpcodes::ATTACK;               // 0x11
    outBuf[3] = (uint8_t)((entityIndex >> 8) & 0xFF);  // D0 = targetHi (índice PURO)
    outBuf[4] = (uint8_t)(entityIndex & 0xFF);         // D1 = targetLo (índice PURO)

    // D2 = 0x78 constante (VALIDADO: push 0x78 no builder; 0x78 em todas as 24 capturas)
    outBuf[5] = 0x78;

    // D3 = direção 0..7 (VALIDADO: [ebp-0x14] & 0x80000007)
    outBuf[6] = ComputeDirection(entityIndex);

    return 7;
}
// Forward declaration
static bool SendPlainPacket(const uint8_t* packet, int len);

// ---- Skill Attack: monta o plaintext e injeta via PacketInjector::Inject ----
// (XorData → SM → C3 0D → Layer-1 → wire; direto, sem fila)
//
// FORMATO CORRETO (decodificado da captura real do cliente S6, 2026-11-15):
//   C1 07 19 [skillH] [skillL] [indexH] [indexL]
//   - skill: 2 bytes = número da skill (ex: 0x003C = 60 = Force do DL)
//   - index: 2 bytes = entity index PURO do monstro (SEM XOR 0xEFF5!)
//   - SEM direction byte (o cliente envia 7 bytes, 1 a menos que a struct do
//     servidor — o CGSkillAttackRecv só lê skill e index, nunca o dis)
//
// Evidência: captura C1 07 19 E7 C3 E2 50 decodificada com o XOR encadeado
// (PacketManager::XorData, m_XorFilter) dá C1 07 19 00 3C 02 77:
//   skill = 0x003C = 60 (SKILL_FORCE), index = 0x0277 = 631 (monstro).
//
// DIFERENTE do ataque normal 0x11 (que vai C1 puro): o skill passa por
// XorData encadeado no payload → SimpleModulus → C3 0D [11] → Layer-1 → wire.
// (PacketInjector::Inject escolhe o pipeline pelo opcode 0x19.)
bool SendSkillAttack(uint16_t entityIndex, uint16_t skillId) {
    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
    uintptr_t targetObj = GameMemory::FindObjectByEntityIndex(entityIndex);
    if (!localPlayer || !targetObj) {
        Logger::Log("ATTACK", "SkillAttack: localPlayer or target null (entityIndex=0x%04X)", entityIndex);
        return false;
    }

    // Log detalhado: comparar OFF_INDEX vs OFF_ID vs slot index
    uint16_t compositeIdx = *(WORD*)(targetObj + GameObject::OFF_INDEX);  // 0x7E
    uint16_t objId = *(WORD*)(targetObj + GameObject::OFF_ID);            // 0x84
    uintptr_t arrayBase = GameMemory::GetObjectArray();
    int slotIndex = (arrayBase && targetObj > arrayBase) ?
                    (int)((targetObj - arrayBase) / GameAddresses::OBJECT_SIZE) : -1;

    Logger::Log("ATTACK", "SkillAttack: entityIndex=0x%04X compositeIdx(OFF_INDEX)=0x%04X objId(OFF_ID)=0x%04X slotIndex=%d skill=0x%04X",
                entityIndex, compositeIdx, objId, slotIndex, skillId);

    // Plaintext real: [C1][07][19][skillH][skillL][indexH][indexL]
    // skill primeiro, depois o entity index PURO (sem 0xEFF5 — o XOR é só do 0x11).
    // Servidor lê: skill = MAKE_NUMBERW(skillH,skillL), bIndex = MAKE_NUMBERW(indexH,indexL).
    uint8_t plain[7];
    plain[0] = MuOpcodes::HEADER_C1;               // 0xC1
    plain[1] = 0x07;                               // size = 7 bytes total
    plain[2] = MuOpcodes::TARGETED_SKILL;          // opcode 0x19
    plain[3] = (uint8_t)((skillId >> 8) & 0xFF);   // skillH
    plain[4] = (uint8_t)(skillId & 0xFF);          // skillL
    plain[5] = (uint8_t)((entityIndex >> 8) & 0xFF); // indexH (entity index puro)
    plain[6] = (uint8_t)(entityIndex & 0xFF);      // indexL

    char hex[32];
    int h = 0;
    for (int i = 0; i < 7; i++)
        h += snprintf(hex + h, sizeof(hex) - h, "%02X ", plain[i]);

    Logger::Log("ATTACK", "SkillAttack: plain=[%s] -> PacketInjector::Inject", hex);

    // Pipeline do skill (diferente do 0x11):
    //   XorData[3,7) → SM → C3 0D [11] → Layer-1 → wire
    return PacketInjector::Inject(plain, 7);
}
static bool SendPlainPacket(const uint8_t* packet, int len) {
    // Injeta DIRETO no socket do game server (sem round-trip via MuProxy2).
    // O buffer e o PLAINTEXT REAL do ataque C1 0x11; PacketInjector::Inject
    // aplica as duas camadas (XorData [3,len) + Layer-1) na ordem correta.
    return PacketInjector::Inject(packet, len);
}

bool SendAttack(uint16_t entityIndex) {
    // Ataque normal (0x11) — funciona bem, sem log por hit (evita poluir o log).
    uint8_t packet[16];
    int len = BuildAttackPacket(packet, entityIndex);

    SendPlainPacket(packet, len);
    return true;
}

bool SendConfiguredAttack(int slotIndex) {
    // Verifica range se configurado (range em tiles, distância em unidades do jogo)
    if (g_SessionRange > 0.0f) {
        float dist = DistanceToSlot(slotIndex);
        float rangeInGameUnits = g_SessionRange * TILE_SIZE;
        if (dist > rangeInGameUnits) {
            return false;
        }
    }

    // Usa o entity index (OFF_INDEX) do slot
    uint16_t entityIndex = GameMemory::GetCompositeIndex(slotIndex);
    if (entityIndex == 0) return false;

    // Envia pacote conforme tipo (skill loga em SendSkillAttack; normal sem log)
    if (g_SessionType == MuOpcodes::ATKTYPE_SKILL) {
        return SendSkillAttack(entityIndex, g_SessionSkillId);
    } else {
        return SendAttack(entityIndex);
    }
}

// ---- Thread por monstro: envia hitCount ataques para um único alvo ----
struct AttackThreadParam {
    uint16_t entityIndex;
    int hitCount;
};

static DWORD WINAPI AttackSingleMobThread(LPVOID param) {
    AttackThreadParam* p = (AttackThreadParam*)param;
    for (int h = 0; h < p->hitCount; h++) {
        // Verifica se o monstro ainda está vivo e no range
        EnterCriticalSection(&g_MapLock);
        bool alive = (g_MonsterMap.find(p->entityIndex) != g_MonsterMap.end());
        LeaveCriticalSection(&g_MapLock);

        if (!alive) {
            Logger::Log("ATTACK", "Thread 0x%04X: monstro morreu/sumiu, abortando hits restantes (%d/%d)",
                        p->entityIndex, h, p->hitCount);
            break;
        }

        if (g_SessionType == MuOpcodes::ATKTYPE_SKILL) {
            SendSkillAttack(p->entityIndex, g_SessionSkillId);
        } else {
            // Alvo exato: o entityIndex já é o alvo correto (validado em 24 capturas nativas).
            // O "alive" acima já filtra por range (g_MonsterMap só contém monstros no range).
            SendAttack(p->entityIndex);
        }
    }
    delete p;
    return 0;
}

// ---- Watcher Thread: escaneia ObjectManager, mantém o map de monstros ativos ----
// Detecta novos monstros (sinaliza Dispatcher) e remove monstros mortos.
static DWORD WINAPI WatcherThread(LPVOID) {
    Logger::Log("ATTACK", "Watcher iniciado");
    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        uintptr_t arrayBase = GameMemory::GetObjectArray();
        uintptr_t localPlayer = GameMemory::GetLocalPlayer();

        if (arrayBase && localPlayer) {
            float lx = *(float*)(localPlayer + GameObject::OFF_X);
            float ly = *(float*)(localPlayer + GameObject::OFF_Y);

            // Coleta entity indices dos monstros vivos no range
            std::set<uint16_t> currentSet;
            for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
                uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
                if (!obj) continue;

                BYTE type = *(BYTE*)(obj + GameObject::OFF_TYPE);
                BYTE live = *(BYTE*)(obj + GameObject::OFF_ISLIVE);
                if (type == ObjType::OBJ_PLAYER || !live) continue;

                uint16_t entityIndex = *(uint16_t*)(obj + GameObject::OFF_INDEX);
                if (entityIndex == 0) continue;

                if (g_SessionRange > 0.0f) {
                    float tx = *(float*)(obj + GameObject::OFF_X);
                    float ty = *(float*)(obj + GameObject::OFF_Y);
                    float dx = tx - lx, dy = ty - ly;
                    float rangeInGameUnits = g_SessionRange * TILE_SIZE;
                    if (sqrtf(dx * dx + dy * dy) > rangeInGameUnits) continue;
                }

                currentSet.insert(entityIndex);
            }

            EnterCriticalSection(&g_MapLock);

            bool changed = false;

            // Remove monstros que morreram/sumiram
            for (auto it = g_MonsterMap.begin(); it != g_MonsterMap.end(); ) {
                if (currentSet.find(it->first) == currentSet.end()) {
                    Logger::Log("ATTACK", "Watcher: monstro 0x%04X removido (morreu/sumiu)", it->first);
                    it = g_MonsterMap.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }

            // Adiciona novos monstros e sinaliza o Dispatcher
            for (auto idx : currentSet) {
                if (g_MonsterMap.find(idx) == g_MonsterMap.end()) {
                    g_MonsterMap[idx] = true;
                    Logger::Log("ATTACK", "Watcher: novo monstro 0x%04X detectado", idx);
                    SetEvent(g_hNewMonsterEvent);  // acorda o Dispatcher
                    changed = true;
                }
            }

            // Emite OBJ via TCP se houve mudanças (popula lista do MuProxy2)
            if (changed) {
                int objCount = 0;
                for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
                    uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
                    if (!obj) continue;

                    BYTE type = *(BYTE*)(obj + GameObject::OFF_TYPE);
                    BYTE live = *(BYTE*)(obj + GameObject::OFF_ISLIVE);
                    if (type == ObjType::OBJ_PLAYER || !live) continue;

                    uint16_t entityIndex = *(uint16_t*)(obj + GameObject::OFF_INDEX);
                    if (entityIndex == 0) continue;

                    // Só emite se está no map (dentro do range)
                    if (g_MonsterMap.find(entityIndex) == g_MonsterMap.end()) continue;

                    char name[32] = {};
                    const char* src = (const char*)(obj + GameObject::OFF_NAME);
                    for (int n = 0; n < 31 && src[n]; ++n) name[n] = src[n];

                    float x = *(float*)(obj + GameObject::OFF_X);
                    float y = *(float*)(obj + GameObject::OFF_Y);
                    float z = *(float*)(obj + GameObject::OFF_Z);
                    short level = 0; // ObjectPreview nao tem level (vem por pacote)

                    float dx = x - lx, dy = y - ly;
                    float dist = sqrtf(dx * dx + dy * dy);

                    CommandChannel::SendFormat("ENTITY|%d|%d|%d|%d|%.1f|%.1f|%.1f|%.1f|%s",
                        i, (int)entityIndex, (int)type, (int)level, x, y, z, dist, name);
                    objCount++;
                }
                CommandChannel::SendFormat("ENTITY_BATCH_END|%d", objCount);
            }

            LeaveCriticalSection(&g_MapLock);
        }

        Sleep(150);  // poll interval do watcher
    }
    Logger::Log("ATTACK", "Watcher encerrado");
    return 0;
}

// ---- Dispatcher Thread: dispara threads de ataque para monstros do map ----
// Acorda quando um novo monstro aparece (event) OU no timeout do delay.
static DWORD WINAPI DispatcherThread(LPVOID) {
    Logger::Log("ATTACK", "Dispatcher iniciado (delay=%dms, range=%.1f, hitcount=%d)",
                g_SessionDelayMs, g_SessionRange, g_SessionHitCount);

    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        // Espera: novo monstro OU timeout do delay
        DWORD waitResult = WaitForSingleObject(g_hNewMonsterEvent, g_SessionDelayMs);

        if (InterlockedCompareExchange(&g_Attacking, 0, 0) != 1) continue;

        int hitCount = g_SessionHitCount;
        if (hitCount < 1) hitCount = 1;

        // Copia o map sob lock para disparar threads fora do lock
        EnterCriticalSection(&g_MapLock);
        int targetCount = (int)g_MonsterMap.size();
        uint16_t* targets = nullptr;
        if (targetCount > 0) {
            targets = new uint16_t[targetCount];
            int idx = 0;
            for (auto& pair : g_MonsterMap) {
                targets[idx++] = pair.first;
            }
        }
        LeaveCriticalSection(&g_MapLock);

        if (targetCount == 0) continue;

        // Fire-and-forget: uma thread por monstro (sem log por ciclo — evita poluir o log)
        for (int t = 0; t < targetCount; t++) {
            auto* p = new AttackThreadParam{ targets[t], hitCount };
            HANDLE h = CreateThread(NULL, 0, AttackSingleMobThread, p, 0, NULL);
            if (h) {
                CloseHandle(h);
            } else {
                delete p;
            }
        }

        delete[] targets;
    }

    Logger::Log("ATTACK", "Dispatcher encerrado");
    return 0;
}

// ---- Parse de key=value: helpers compartilhados em CommandParams.h ----

// ============================================================================
// Handlers de mensagens tipadas (registrados via REGISTER_MESSAGE)
// ============================================================================

static void HandleAttackStart(const char* payload) {
    // payload: "delay=500|range=8|type=0|hitcount=3|skill=1E"
    g_SessionDelayMs = g_Config.defaultDelayMs;
    g_SessionRange   = g_Config.defaultRange;
    g_SessionType    = g_Config.defaultType;
    g_SessionSkillId = g_Config.defaultSkillId;
    g_SessionHitCount = g_Config.defaultHitCount;

    if (payload && *payload) {
        int ival; float fval; uint16_t hval;
        if (CommandParams::ParseIntParam(payload, "delay", &ival) && ival > 0) g_SessionDelayMs = ival;
        if (CommandParams::ParseFloatParam(payload, "range", &fval) && fval > 0.0f) g_SessionRange = fval;
        if (CommandParams::ParseHexParam(payload, "skill", &hval)) {
            g_SessionSkillId = hval;
            if (g_SessionType == 0) g_SessionType = MuOpcodes::ATKTYPE_SKILL;
        }
        CommandParams::ParseByteParam(payload, "type", &g_SessionType);
        if (CommandParams::ParseIntParam(payload, "hitcount", &ival) && ival >= 1) g_SessionHitCount = ival;
    }

    InterlockedExchange(&g_Attacking, 1);
    Logger::Log("ATTACK", "START auto-attack delay=%dms range=%.1f type=%s skill=0x%04X hitcount=%d",
                g_SessionDelayMs, g_SessionRange,
                g_SessionType ? "SKILL" : "NORMAL", g_SessionSkillId, g_SessionHitCount);
    DebugTools::DumpObjectManager();
}
REGISTER_MESSAGE(ATTACK_START, HandleAttackStart);

static void HandleAttackStop(const char* payload) {
    (void)payload;
    InterlockedExchange(&g_Attacking, 0);
    Logger::Log("ATTACK", "Ataque contínuo parado");
}
REGISTER_MESSAGE(ATTACK_STOP, HandleAttackStop);

static void HandleAttackConfig(const char* payload) {
    if (!payload || !*payload) return;
    int ival; float fval; uint16_t hval;
    if (CommandParams::ParseIntParam(payload, "delay", &ival) && ival > 0) {
        g_Config.defaultDelayMs = ival;
        g_SessionDelayMs = ival;
    }
    if (CommandParams::ParseFloatParam(payload, "range", &fval)) {
        g_Config.defaultRange = fval;
        g_SessionRange = fval;
    }
    if (CommandParams::ParseByteParam(payload, "type", &g_Config.defaultType))
        g_SessionType = g_Config.defaultType;
    if (CommandParams::ParseHexParam(payload, "skill", &hval)) {
        g_Config.defaultSkillId = hval;
        g_SessionSkillId = hval;
        if (g_SessionType == 0) g_SessionType = MuOpcodes::ATKTYPE_SKILL;
    }
    if (CommandParams::ParseIntParam(payload, "hitcount", &ival) && ival >= 1) {
        g_Config.defaultHitCount = ival;
        g_SessionHitCount = ival;
    }
    Logger::Log("ATTACK", "CONFIG atualizado: delay=%dms range=%.1f type=%s skill=0x%04X hitcount=%d",
                g_SessionDelayMs, g_SessionRange,
                g_SessionType ? "SKILL" : "NORMAL", g_SessionSkillId, g_SessionHitCount);
}
REGISTER_MESSAGE(ATTACK_CONFIG, HandleAttackConfig);

static void HandleAttackPlayerIdx(const char* payload) {
    uint16_t playerIdx = 0;
    if (CommandParams::ParseHexParam(payload, "idx", &playerIdx) && playerIdx != 0) {
        GameMemory::SetLocalPlayerIndex(playerIdx);
        Logger::Log("ATTACK", "PLAYERIDX setado: 0x%04X", playerIdx);
    } else {
        unsigned int val = 0;
        if (sscanf_s(payload, "%x", &val) == 1 && val != 0) {
            GameMemory::SetLocalPlayerIndex((uint16_t)val);
            Logger::Log("ATTACK", "PLAYERIDX setado (direto): 0x%04X", (uint16_t)val);
        }
    }
}
REGISTER_MESSAGE(ATTACK_PLAYERIDX, HandleAttackPlayerIdx);

// Handler para SEND_PACKET (injeção de pacote direto no game server)
static void HandleSendPacket(const char* payload) {
    // payload: "Server|C10711ED1F4486" (direção ignorada, sempre injeta C->S)
    // IMPORTANTE: o hex é a forma XorData-ENCRYPTED (a que o byp loga / o usuario
    // copia do log), NAO o plaintext real. InjectXorData aplica so Layer-1.
    // Ex.: C1 07 11 ED 1F 44 86 = XorData de C1 07 11 02 EE 78 07 (D2=0x78, D3=0x07).
    // Para injetar o plaintext REAL de um ataque, use o SendAttack (PacketInjector::Inject).
    const char* hex = strchr(payload, '|');
    if (!hex) return;
    hex++; // pula o '|'

    // Converte hex string para bytes
    uint8_t packet[2048];
    int packetLen = 0;
    while (*hex && packetLen < (int)sizeof(packet)) {
        while (*hex == ' ') hex++;
        if (!*hex || !*(hex + 1)) break;
        unsigned int val;
        char tmp[3] = { hex[0], hex[1], 0 };
        if (sscanf_s(tmp, "%02X", &val) == 1)
            packet[packetLen++] = (uint8_t)val;
        hex += 2;
    }

    if (packetLen > 0) {
        // Injeta DIRETO no socket do game server (sem round-trip via MuProxy2)
        if (PacketInjector::InjectXorData(packet, packetLen)) {
            Logger::Log("ATTACK", "SEND_PACKET: %d bytes injetados direto no GS", packetLen);
        } else {
            Logger::Log("ATTACK", "SEND_PACKET: FALHA ao injetar %d bytes (GS socket nao rastreado)", packetLen);
        }
    }
}
REGISTER_MESSAGE(SEND_PACKET, HandleSendPacket);

// Handler para MONSTER_START
static void HandleMonsterStart(const char* payload) {
    // payload: "interval=500|filter=6"
    // Formato esperado pelo MonsterTracker: MONSTER|START interval=500 filter=6
    char buf[256];
    snprintf(buf, sizeof(buf), "MONSTER|START %s", payload ? payload : "");
    // Substitui '|' por ' ' nos parametros (mantem o primeiro '|' como separador)
    char* firstPipe = strchr(buf, '|');
    if (firstPipe) {
        for (char* p = firstPipe + 1; *p; p++) if (*p == '|') *p = ' ';
    }
    PacketHandler::ProcessLine(buf, (int)strlen(buf));
}
REGISTER_MESSAGE(MONSTER_START, HandleMonsterStart);

// Handler para MONSTER_STOP
static void HandleMonsterStop(const char* payload) {
    (void)payload;
    PacketHandler::ProcessLine("MONSTER|STOP", 12);
}
REGISTER_MESSAGE(MONSTER_STOP, HandleMonsterStop);

bool Initialize() {
    InitializeCriticalSection(&g_MapLock);
    InitializeCriticalSection(&g_smCounterLock);
    g_hNewMonsterEvent = CreateEventA(NULL, FALSE, FALSE, NULL);  // auto-reset

    g_Running = 1;

    g_hWatcherThread = CreateThread(NULL, 0, WatcherThread, NULL, 0, NULL);
    g_hDispatcherThread = CreateThread(NULL, 0, DispatcherThread, NULL, 0, NULL);

    if (!g_hWatcherThread || !g_hDispatcherThread) {
        Logger::Log("ATTACK", "FALHA ao criar threads (Watcher ou Dispatcher)");
        g_Running = 0;
        if (g_hWatcherThread) { CloseHandle(g_hWatcherThread); g_hWatcherThread = nullptr; }
        if (g_hDispatcherThread) { CloseHandle(g_hDispatcherThread); g_hDispatcherThread = nullptr; }
        return false;
    }

    Logger::Log("ATTACK", "Inicializado (default: delay=%dms range=%.1f type=%s hitcount=%d)",
                g_Config.defaultDelayMs, g_Config.defaultRange,
                g_Config.defaultType ? "SKILL" : "NORMAL", g_Config.defaultHitCount);
    return true;
}

void Shutdown() {
    InterlockedExchange(&g_Attacking, 0);
    InterlockedExchange(&g_Running, 0);

    // Acorda o Dispatcher para ele sair do WaitForSingleObject
    if (g_hNewMonsterEvent) SetEvent(g_hNewMonsterEvent);

    if (g_hWatcherThread) {
        WaitForSingleObject(g_hWatcherThread, 3000);
        CloseHandle(g_hWatcherThread);
        g_hWatcherThread = nullptr;
    }
    if (g_hDispatcherThread) {
        WaitForSingleObject(g_hDispatcherThread, 3000);
        CloseHandle(g_hDispatcherThread);
        g_hDispatcherThread = nullptr;
    }
    if (g_hNewMonsterEvent) {
        CloseHandle(g_hNewMonsterEvent);
        g_hNewMonsterEvent = nullptr;
    }
    DeleteCriticalSection(&g_MapLock);
    DeleteCriticalSection(&g_smCounterLock);

    Logger::Log("ATTACK", "Desligado");
}

} // namespace AttackModule