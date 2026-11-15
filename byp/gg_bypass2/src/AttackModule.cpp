// AttackModule.cpp — Monta e envia pacotes de ataque (normal + skill)
#include "include/AttackModule.h"
#include "include/MuOpcodes.h"
#include "include/GameMemory.h"
#include "include/GameObject.h"
#include "include/ObjType.h"
#include "include/GameAddresses.h"
#include "include/TcpClient.h"
#include "include/PacketHandler.h"
#include "include/MessageRegistry.h"
#include "include/PacketSniffer.h"
#include "include/SkillMap.h"
#include "include/Logger.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
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
    0xE7, // defaultSkillId (hardcoded: skill 0xE7)
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

    Logger::Log("ATTACK", "ComputeDir: entity=0x%04X p(%.0f,%.0f) t(%.0f,%.0f) dx=%.0f dy=%.0f angle=%.3f dir=%d dirByte=0x%02X",
                targetEntityIndex, playerX, playerY, targetX, targetY, dx, dy, angle, dirInt, dirByte);

    return dirByte;
}

// ---- Calcula byte de direção para skill attack (formato 0-255) ----
// O jogo usa fórmula diferente para skill attacks:
//   direction = ROUND(angle_degrees / 360.0 * 256.0) & 0xFF
// Onde angle_degrees é calculado por atan2(dy, dx) convertido para graus.
// Resultado: 0-255 (sem o offset 0xC0 dos ataques normais)
// Evidência: pacote real C1 07 19 E7 EE CF 12 → direction = 0x12 (18 decimal)
static uint8_t ComputeSkillDirection(uint16_t targetEntityIndex) {
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

    // Fórmula do jogo para skill attacks:
    // angle = atan2(dy, dx) em radianos → converter para graus
    // direction = ROUND(angle_deg / 360.0 * 256.0) & 0xFF
    // O jogo adiciona +180 antes de /360 se necessário (direction_byte_1)
    // Mas o byte que vai no pacote é direction_byte_2 (sem +180)
    float angleRad = atan2f(dy, dx);
    float angleDeg = angleRad * 180.0f / 3.14159265f;
    if (angleDeg < 0) angleDeg += 360.0f;

    // direction_byte_2 = (angle / 360.0 * 256.0) rounded, masked to byte
    float dirFloat = angleDeg / 360.0f * 256.0f;
    int dirInt = (int)(dirFloat + 0.5f);  // round
    if (dirInt < 0) dirInt += 256;
    uint8_t dirByte = (uint8_t)(dirInt & 0xFF);

    Logger::Log("ATTACK", "ComputeSkillDir: entity=0x%04X p(%.0f,%.0f) t(%.0f,%.0f) dx=%.0f dy=%.0f angleDeg=%.1f dirInt=%d dirByte=0x%02X",
                targetEntityIndex, playerX, playerY, targetX, targetY, dx, dy, angleDeg, dirInt, dirByte);

    return dirByte;
}

// ---- Monta pacote de ataque normal 0x11 (PLAINTEXT REAL, pré-criptografia) ----
// Formato VALIDADO (RE 2 camadas, 24 capturas nativas do byp_packets.log):
//   C1 07 11 <targetHi> <targetLo> 78 <dir 0..7>
//   - target = EntityIndex PURO (SEM XOR 0xEFF5!), big-endian.
//     VALIDADO em campo: o builder nativo coloca o índice cru em D0D1
//     (monstro 0x001A -> D0D1 = 00 1A em TODAS as capturas; D2=0x78 e
//     D3 em 0..7 nos 4 alvos diferentes 0x001A/0x001B/0x0025/0x005A).
//     O XOR 0xEFF5 é convenção de OUTROS opcodes (skill 0x19), não do 0x11.
//   - D2 = 0x78 CONSTANTE (disasm builder 0x5A1701: push 0x78; 0x78 em todas as 24 capturas)
//   - D3 = direção 0..7 (disasm: [ebp-0x14] & 0x80000007)
// Este buffer é o PLAINTEXT REAL. A criptografia (XorData [3,7) + Layer-1)
// é aplicada em InjectC1Attack (PacketSniffer.cpp).
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

// ---- Skill Attack: enfileira plaintext para HookSend processar na thread do jogo ----
// MANUAL PATH: Forma C1 plaintext → SM encrypt → C3 → Layer-1 → send
//
// FORMATO CORRETO (confirmado via trace x64dbg — 4 addData calls):
//   C1 07 19 [skillH] [skillL] [targetH] [targetL]
//   - skill: 2 bytes (skill number completo, ex: 0x01E7 = Inferno)
//   - target: 2 bytes, XOR'd with 0xEFF5 (AttackTargetID)
//     The game reads this from global 0x081C03A4, but the value stored there
//     is already entityIndex XOR 0xEFF5.
//   - SEM direction byte (o jogo não envia)
//
// Evidência: pacote real do jogo: C1 07 19 E7 C3 E2 CF
//   skillH=0xE7, skillL=0xC3 → skill=0xE7C3
//   targetH=0xE2, targetL=0xCF → target=0xE2CF, 0xE2CF ^ 0xEFF5 = 0x0D3A (entity index)
//
// O valor mostrado na coluna "Atk ID" do MuProxy2 é entityIndex ^ 0xEFF5.
// Exemplo: monstro visto como EBEB na UI → entityIndex = 0xEBEB ^ 0xEFF5 = 0x0218.
// Portanto o pacote deve enviar 0xEBEB como target (XOR'd), NÃO 0x0218.
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

    // Target encoding for skill attacks:
    // - Manual skill packet format: C1 07 19 [skill(1)] [targetH] [targetL] [dir(1)]
    // - The game reads global 0x081C03A4 directly into the packet bytes.
    // - The value stored at 0x081C03A4 is entityIndex XOR 0xEFF5.
    // - Confirmed: MuProxy2 UI shows AttackId = entityIndex ^ 0xEFF5 (e.g. 0xEBEB for entity 0x0218).
    // - Therefore we MUST send entityIndex XOR 0xEFF5 as target.
    uint16_t attackIdDirect = entityIndex;
    uint16_t attackIdXor = GameMemory::EntityToAttackId(entityIndex); // entityIndex ^ 0xEFF5

    // Use XOR'd version — matches the value the game stores at 0x081C03A4
    uint16_t attackId = attackIdXor;
    uint8_t direction = ComputeSkillDirection(entityIndex);

    uint16_t gameTargetGlobal = GameMemory::GetTargetGlobal();
    int32_t gameTargetSlot = GameMemory::GetTargetSlotGlobal();

    // Log ALL target encodings for diagnostic comparison
    Logger::Log("ATTACK", "SkillAttack: entity=0x%04X DIRECT=0x%04X XOR=0x%04X gameGlobal=0x%04X gameSlot=%d dir=0x%02X skill=0x%04X",
                entityIndex, attackIdDirect, attackIdXor, gameTargetGlobal, gameTargetSlot, direction, skillId);

    // Server format (GAMESERVER_UPDATE>=701, SkillManager.h PMSG_SKILL_ATTACK_RECV):
    //   C1 07 19 [indexH] [skillH] [indexL] [skillL]
    // Bytes INTERCALADOS: index e skill alternam H/L.
    // Servidor lê: bIndex = MAKE_NUMBERW(indexH,indexL), skill = MAKE_NUMBERW(skillH,skillL)
    uint8_t smInput[7];
    smInput[0] = 0xC1;                                          // header C1
    smInput[1] = 0x07;                                          // size = 7 bytes total
    smInput[2] = MuOpcodes::TARGETED_SKILL;                     // opcode 0x19
    smInput[3] = (uint8_t)((attackId >> 8) & 0xFF);            // indexH
    smInput[4] = (uint8_t)((skillId >> 8) & 0xFF);             // skillH
    smInput[5] = (uint8_t)(attackId & 0xFF);                    // indexL
    smInput[6] = (uint8_t)(skillId & 0xFF);                     // skillL

    char hex[32];
    int h = 0;
    for (int i = 0; i < 7; i++)
        h += snprintf(hex + h, sizeof(hex) - h, "%02X ", smInput[i]);

    Logger::Log("ATTACK", "SkillAttack: plain=[%s] -> enqueue MANUAL", hex);

    // Enfileira com tipo MANUAL: forma C1 payload → SM encrypt → C3 → Layer-1 → send
    return PacketSniffer::EnqueueSkillPacket(smInput, 7, PacketSniffer::SKILL_TYPE_MANUAL, skillId, attackId);
}
static bool SendPlainPacket(const uint8_t* packet, int len) {
    // Injeta DIRETO no socket do game server (sem round-trip via MuProxy2).
    // O buffer e o PLAINTEXT REAL do ataque C1 0x11; InjectC1Attack aplica
    // as duas camadas (XorData [3,len) + Layer-1) na ordem correta.
    return PacketSniffer::InjectC1Attack(packet, len);
}

bool SendAttack(uint16_t entityIndex) {
    uint16_t attackId = GameMemory::EntityToAttackId(entityIndex);
    uint16_t gameTargetGlobal = GameMemory::GetTargetGlobal();
    int32_t gameTargetSlot = GameMemory::GetTargetSlotGlobal();
    uint8_t packet[16];
    int len = BuildAttackPacket(packet, entityIndex);

    // Log detalhado incluindo action e direction bytes
    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
    float rawAngle = localPlayer ? *(float*)(localPlayer + GameObject::OFF_DIR_ANGLE) : -1.0f;
    Logger::Log("ATTACK", "Ataque normal: entity=0x%04X attackId(XOR)=0x%04X DIRECT=0x%04X gameGlobal=0x%04X gameSlot=%d action=0x%02X dir=0x%02X angle=%.1f",
                entityIndex, attackId, entityIndex, gameTargetGlobal, gameTargetSlot, packet[5], packet[6], rawAngle);

    // Hex dump do pacote para verificação
    char hexBuf[32];
    int pos = 0;
    for (int i = 0; i < len && pos + 3 < (int)sizeof(hexBuf); ++i)
        pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "%02X ", (unsigned char)packet[i]);
    Logger::Log("ATTACK", "Pacote attack: [%s]", hexBuf);

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
    uintptr_t obj = GameMemory::GetObjectByIndex(slotIndex);
    uint16_t entityIndex = GameMemory::GetCompositeIndex(slotIndex);
    if (entityIndex == 0) return false;

    // Log diagnostic: entity index vs game's target global
    uint16_t gameTargetGlobal = GameMemory::GetTargetGlobal();
    int32_t gameTargetSlot = GameMemory::GetTargetSlotGlobal();
    Logger::Log("ATTACK", "SendConfiguredAttack: slot=%d entityIdx=0x%04X gameGlobal=0x%04X gameSlot=%d XOR=0x%04X",
                slotIndex, entityIndex, gameTargetGlobal, gameTargetSlot, GameMemory::EntityToAttackId(entityIndex));

    // Envia pacote conforme tipo
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

                    TcpClient::SendFormat("ENTITY|%d|%d|%d|%d|%.1f|%.1f|%.1f|%.1f|%s",
                        i, (int)entityIndex, (int)type, (int)level, x, y, z, dist, name);
                    objCount++;
                }
                TcpClient::SendFormat("ENTITY_BATCH_END|%d", objCount);
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

        // Fire-and-forget: uma thread por monstro
        for (int t = 0; t < targetCount; t++) {
            auto* p = new AttackThreadParam{ targets[t], hitCount };
            HANDLE h = CreateThread(NULL, 0, AttackSingleMobThread, p, 0, NULL);
            if (h) {
                CloseHandle(h);
            } else {
                delete p;
            }
        }

        Logger::Log("ATTACK", "Dispatcher: %d monstros x %d hits (fire-and-forget)", targetCount, hitCount);
        delete[] targets;
    }

    Logger::Log("ATTACK", "Dispatcher encerrado");
    return 0;
}

// ---- Utilitários: parse de key=value em strings ----
static bool ParseIntParam(const char* s, const char* key, int* outVal) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(s, search);
    if (!p) return false;
    p += strlen(search);
    *outVal = atoi(p);
    return true;
}

static bool ParseByteParam(const char* s, const char* key, uint8_t* outVal) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(s, search);
    if (!p) return false;
    p += strlen(search);
    int val = atoi(p);
    *outVal = (uint8_t)val;
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

static bool ParseHexParam(const char* s, const char* key, uint16_t* outVal) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(s, search);
    if (!p) return false;
    p += strlen(search);
    unsigned int val;
    if (sscanf_s(p, "%x", &val) == 1) {
        *outVal = (uint16_t)val;
        return true;
    }
    return false;
}

// ==== Lê DWORD com proteção contra access violation ====
static DWORD SafeReadDword(uintptr_t addr) {
    __try { return *(DWORD*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xDEADDEAD; }
}
static WORD SafeReadWord(uintptr_t addr) {
    __try { return *(WORD*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xDEAD; }
}
static BYTE SafeReadByte(uintptr_t addr) {
    __try { return *(BYTE*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xFF; }
}
static float SafeReadFloat(uintptr_t addr) {
    __try { return *(float*)addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -9999.0f; }
}

// ==== DIAGNÓSTICO: despeja o ObjectManager completo com viewport scan ====
static void DumpObjectManager() {
    uintptr_t arrayBase = GameMemory::GetObjectArray();
    if (!arrayBase) {
        Logger::Log("ATTACK", "DUMP: ObjectArray null!");
        return;
    }

    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
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
        GameMemory::GetName(i, name, sizeof(name));

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

// ==== HEX DUMP: despeja o slot inteiro (0x598 bytes) com anotações ====
static void HexDumpSlot(int slotIndex) {
    uintptr_t arrayBase = GameMemory::GetObjectArray();
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
    GameMemory::GetName(slotIndex, name, sizeof(name));

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

// ==== SCANVALUE: busca um valor WORD em todos os 400 slots do ObjectManager ====
// Procura por attack IDs, entity indexes, ou qualquer valor relevante.
// Escaneia TODOS os slots (mesmo vazios) em busca do valor exato.
static void ScanForValue(uint16_t searchValue) {
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

// ==== SCANSLOT: busca um valor WORD em UM slot específico (hex dump + busca) ====
static void ScanSlotForValue(int slotIndex, uint16_t searchValue) {
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

// ==== DIRSCAN: escaneia o player local buscando o byte de direção ====
// Procura por bytes com valor 0-7 (direções em 8-dir) ou 0-15 (16-dir)
// em offsets candidatos no objeto do jogador local.
static void DirScan() {
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

static void HandleAttackStart(const char* payload) {
    // payload: "delay=500|range=8|type=0|hitcount=3|skill=1E"
    g_SessionDelayMs = g_Config.defaultDelayMs;
    g_SessionRange   = g_Config.defaultRange;
    g_SessionType    = g_Config.defaultType;
    g_SessionSkillId = g_Config.defaultSkillId;
    g_SessionHitCount = g_Config.defaultHitCount;

    if (payload && *payload) {
        int ival; float fval; uint16_t hval;
        if (ParseIntParam(payload, "delay", &ival) && ival > 0) g_SessionDelayMs = ival;
        if (ParseFloatParam(payload, "range", &fval) && fval > 0.0f) g_SessionRange = fval;
        if (ParseHexParam(payload, "skill", &hval)) {
            g_SessionSkillId = hval;
            if (g_SessionType == 0) g_SessionType = MuOpcodes::ATKTYPE_SKILL;
        }
        ParseByteParam(payload, "type", &g_SessionType);
        if (ParseIntParam(payload, "hitcount", &ival) && ival >= 1) g_SessionHitCount = ival;
    }

    InterlockedExchange(&g_Attacking, 1);
    Logger::Log("ATTACK", "START auto-attack delay=%dms range=%.1f type=%s skill=0x%04X hitcount=%d",
                g_SessionDelayMs, g_SessionRange,
                g_SessionType ? "SKILL" : "NORMAL", g_SessionSkillId, g_SessionHitCount);
    DumpObjectManager();
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
    if (ParseIntParam(payload, "delay", &ival) && ival > 0) {
        g_Config.defaultDelayMs = ival;
        g_SessionDelayMs = ival;
    }
    if (ParseFloatParam(payload, "range", &fval)) {
        g_Config.defaultRange = fval;
        g_SessionRange = fval;
    }
    if (ParseByteParam(payload, "type", &g_Config.defaultType))
        g_SessionType = g_Config.defaultType;
    if (ParseHexParam(payload, "skill", &hval)) {
        g_Config.defaultSkillId = hval;
        g_SessionSkillId = hval;
        if (g_SessionType == 0) g_SessionType = MuOpcodes::ATKTYPE_SKILL;
    }
    if (ParseIntParam(payload, "hitcount", &ival) && ival >= 1) {
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
    if (ParseHexParam(payload, "idx", &playerIdx) && playerIdx != 0) {
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

static void HandleAttackDump(const char* payload) {
    (void)payload;
    DumpObjectManager();
}
REGISTER_MESSAGE(ATTACK_DUMP, HandleAttackDump);

// Handler para SEND_PACKET (injeção de pacote direto no game server)
static void HandleSendPacket(const char* payload) {
    // payload: "Server|C10711ED1F4486" (direção ignorada, sempre injeta C->S)
    // IMPORTANTE: o hex é a forma XorData-ENCRYPTED (a que o byp loga / o usuario
    // copia do log), NAO o plaintext real. InjectToServer aplica so Layer-1.
    // Ex.: C1 07 11 ED 1F 44 86 = XorData de C1 07 11 02 EE 78 07 (D2=0x78, D3=0x07).
    // Para injetar o plaintext REAL de um ataque, use o SendAttack (InjectC1Attack).
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
        if (PacketSniffer::InjectToServer(packet, packetLen)) {
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