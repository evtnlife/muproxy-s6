// AttackModule.cpp — Monta e envia pacotes de ataque (normal + skill)
#include "AttackModule.h"
#include "MuOpcodes.h"
#include "GameMemory/Object.h"
#include "GameMemory/Player.h"
#include "GameMemory/Globals.h"
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

// ---- Target Map: entityIndex -> type (protegido por CriticalSection) ----
static std::map<uint16_t, uint8_t> g_MonsterMap;  // entityIndex -> ObjType (1=player, 2=monstro, 4=NPC, ...)
static CRITICAL_SECTION g_MapLock;
static HANDLE g_hNewMonsterEvent = nullptr;  // sinalizado quando novo alvo aparece
static HANDLE g_hWatcherThread = nullptr;
static HANDLE g_hDispatcherThread = nullptr;

// ---- Configurações dinâmicas ----
static AttackConfig g_Config = {
    100,    // defaultDelayMs
    0.0f,   // defaultRange (0 = sem limite)
    0,      // defaultType (0 = normal)
    0x003C, // defaultSkillId (60 = SKILL_FORCE do DL — número real da skill)
    1,      // defaultHitCount
    true,   // defaultAttackMonsters (atacar monstros)
    false,  // defaultAttackPlayersAll (atacar todos os jogadores)
    false,  // defaultAttackPlayersClick (atacar jogador no clique)
    false   // defaultAttackOthers (atacar outros)
};

// ---- Parâmetros da sessão de ataque ativa ----
static int     g_SessionDelayMs = 800;
static float   g_SessionRange   = 0.0f;
static uint8_t g_SessionType    = 0;      // 0=normal, 1=skill
static uint16_t g_SessionSkillId = 0;
static int     g_SessionHitCount = 1;     // hits por alvo por ciclo
static volatile LONG g_SessionAttackMonsters    = 1;  // 1 = atacar monstros (type 2)
static volatile LONG g_SessionAttackPlayersAll  = 0;  // 1 = atacar todos os jogadores (type 1)
static volatile LONG g_SessionAttackPlayersClick = 0; // 1 = atacar jogador no clique
static volatile LONG g_SessionAttackOthers      = 0;  // 1 = atacar outros (não 1, não 2)

AttackConfig& GetConfig() { return g_Config; }

// ---- Distância entre local player e o slot do alvo ----
static float DistanceToSlot(int slotIndex) {
    uintptr_t localPlayer = GameMemory::Player::Object();
    if (!localPlayer) return 9999.0f;

    float lx = *(float*)(localPlayer + GameObject::OFF_X);
    float ly = *(float*)(localPlayer + GameObject::OFF_Y);

    uintptr_t obj = GameMemory::Object::ByIndex(slotIndex);
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
    uintptr_t localPlayer = GameMemory::Player::Object();
    if (!localPlayer) return 0;

    uintptr_t targetObj = GameMemory::Object::FindByEntityIndex(targetEntityIndex);
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
    outBuf[0] = (uint8_t)MuOpcodes::HeaderType::C1;           // 0xC1
    outBuf[1] = MuOpcodes::Size::AttackNormal;                 // 0x07
    outBuf[2] = (uint8_t)MuOpcodes::ClientToServer::Attack;    // 0x11
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
// ============================================================================
// Throttle do log no caminho quente.
//
// Com o auto-ataque ligado, cada skill gerava ~6 linhas (2 aqui + 4 no
// PacketInjector) e o log passava de 60 MB em poucos minutos — com fflush
// por linha, a thread do JOGO ficava esperando o lock global do logger e
// o client travava. Agora o detalhe sai 1 a cada 20 ataques.
// ============================================================================
static volatile LONG g_AttackLogCounter = 0;
static inline bool ShouldLogAttackDetail() {
    return (InterlockedIncrement(&g_AttackLogCounter) % 20) == 1;
}

bool SendSkillAttack(uint16_t entityIndex, uint16_t skillId) {
    uintptr_t localPlayer = GameMemory::Player::Object();
    uintptr_t targetObj = GameMemory::Object::FindByEntityIndex(entityIndex);
    if (!localPlayer || !targetObj) {
        Logger::Log("ATTACK", "SkillAttack: localPlayer or target null (entityIndex=0x%04X)", entityIndex);
        return false;
    }

    const bool logDetail = ShouldLogAttackDetail();

    // Log detalhado: comparar OFF_INDEX vs OFF_ID vs slot index
    uint16_t compositeIdx = *(WORD*)(targetObj + GameObject::OFF_INDEX);  // 0x7E
    uint16_t objId = *(WORD*)(targetObj + GameObject::OFF_ID);            // 0x84
    uintptr_t arrayBase = GameMemory::Object::Array();
    int slotIndex = (arrayBase && targetObj > arrayBase) ?
                    (int)((targetObj - arrayBase) / GameAddresses::OBJECT_SIZE) : -1;

    if (logDetail) {
        Logger::Log("ATTACK", "SkillAttack: entityIndex=0x%04X compositeIdx(OFF_INDEX)=0x%04X objId(OFF_ID)=0x%04X slotIndex=%d skill=0x%04X",
                    entityIndex, compositeIdx, objId, slotIndex, skillId);
    }

    // Plaintext real: [C1][07][19][skillH][skillL][indexH][indexL]
    // skill primeiro, depois o entity index PURO (sem 0xEFF5 — o XOR é só do 0x11).
    // Servidor lê: skill = MAKE_NUMBERW(skillH,skillL), bIndex = MAKE_NUMBERW(indexH,indexL).
    uint8_t plain[7];
    plain[0] = (uint8_t)MuOpcodes::HeaderType::C1;             // 0xC1
    plain[1] = MuOpcodes::Size::TargetedSkill;                 // size = 7 bytes total
    plain[2] = (uint8_t)MuOpcodes::ClientToServer::TargetedSkill; // opcode 0x19
    plain[3] = (uint8_t)((skillId >> 8) & 0xFF);   // skillH
    plain[4] = (uint8_t)(skillId & 0xFF);          // skillL
    plain[5] = (uint8_t)((entityIndex >> 8) & 0xFF); // indexH (entity index puro)
    plain[6] = (uint8_t)(entityIndex & 0xFF);      // indexL

    char hex[32];
    int h = 0;
    for (int i = 0; i < 7; i++)
        h += snprintf(hex + h, sizeof(hex) - h, "%02X ", plain[i]);

    if (logDetail) {
        Logger::Log("ATTACK", "SkillAttack: plain=[%s] -> PacketInjector::Inject", hex);
    }

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
    uint16_t entityIndex = GameMemory::Object::CompositeIndex(slotIndex);
    if (entityIndex == 0) return false;

    // Envia pacote conforme tipo (skill loga em SendSkillAttack; normal sem log)
    if (g_SessionType == (uint8_t)MuOpcodes::AttackType::Skill) {
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

        if (g_SessionType == (uint8_t)MuOpcodes::AttackType::Skill) {
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

// ---- Watcher Thread: escaneia ObjectManager, mantém o map de alvos ativos ----
// Detecta novos alvos (monstros + jogadores) e sinaliza o Dispatcher.
static DWORD WINAPI WatcherThread(LPVOID) {
    Logger::Log("ATTACK", "Watcher iniciado");
    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        uintptr_t arrayBase = GameMemory::Object::Array();
        uintptr_t localPlayer = GameMemory::Player::Object();

        if (arrayBase && localPlayer) {
            float lx = *(float*)(localPlayer + GameObject::OFF_X);
            float ly = *(float*)(localPlayer + GameObject::OFF_Y);

            // Coleta entity indices e tipos dos alvos vivos no range
            // (monstros, jogadores e/ou outros, conforme os checkboxes)
            std::map<uint16_t, uint8_t> currentSet;
            for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
                uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
                if (!obj) continue;

                BYTE type = *(BYTE*)(obj + GameObject::OFF_TYPE);
                BYTE live = *(BYTE*)(obj + GameObject::OFF_ISLIVE);
                if (!live) continue;

                bool include = false;
                if (type == ObjType::OBJ_MONSTER && g_SessionAttackMonsters) include = true;
                else if (type == ObjType::OBJ_PLAYER && (g_SessionAttackPlayersAll || g_SessionAttackPlayersClick)) include = true;
                else if (type != ObjType::OBJ_MONSTER && type != ObjType::OBJ_PLAYER && g_SessionAttackOthers) include = true;
                if (!include) continue;

                uint16_t entityIndex = *(uint16_t*)(obj + GameObject::OFF_INDEX);
                if (entityIndex == 0) continue;

                if (g_SessionRange > 0.0f) {
                    float tx = *(float*)(obj + GameObject::OFF_X);
                    float ty = *(float*)(obj + GameObject::OFF_Y);
                    float dx = tx - lx, dy = ty - ly;
                    float rangeInGameUnits = g_SessionRange * TILE_SIZE;
                    if (sqrtf(dx * dx + dy * dy) > rangeInGameUnits) continue;
                }

                currentSet[entityIndex] = type;
            }

            EnterCriticalSection(&g_MapLock);

            bool changed = false;

            // Remove alvos que morreram/sumiram
            for (auto it = g_MonsterMap.begin(); it != g_MonsterMap.end(); ) {
                if (currentSet.find(it->first) == currentSet.end()) {
                    Logger::Log("ATTACK", "Watcher: alvo 0x%04X removido (morreu/sumiu)", it->first);
                    it = g_MonsterMap.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }

            // Adiciona novos alvos e sinaliza o Dispatcher
            for (auto& pair : currentSet) {
                if (g_MonsterMap.find(pair.first) == g_MonsterMap.end()) {
                    g_MonsterMap[pair.first] = pair.second;
                    Logger::Log("ATTACK", "Watcher: novo alvo 0x%04X detectado (type=%d)", pair.first, pair.second);
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
                    if (!live) continue;

                    bool include = false;
                    if (type == ObjType::OBJ_MONSTER && g_SessionAttackMonsters) include = true;
                    else if (type == ObjType::OBJ_PLAYER && (g_SessionAttackPlayersAll || g_SessionAttackPlayersClick)) include = true;
                    else if (type != ObjType::OBJ_MONSTER && type != ObjType::OBJ_PLAYER && g_SessionAttackOthers) include = true;
                    if (!include) continue;

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
    Logger::Log("ATTACK", "Dispatcher iniciado (delay=%dms, range=%.1f, hitcount=%d, monsters=%d, playersAll=%d, playersClick=%d, others=%d)",
                g_SessionDelayMs, g_SessionRange, g_SessionHitCount,
                (int)g_SessionAttackMonsters, (int)g_SessionAttackPlayersAll,
                (int)g_SessionAttackPlayersClick, (int)g_SessionAttackOthers);

    DWORD lastAttackTick = 0;

    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        // Espera: novo monstro OU timeout do delay
        DWORD waitResult = WaitForSingleObject(g_hNewMonsterEvent, g_SessionDelayMs);

        if (InterlockedCompareExchange(&g_Attacking, 0, 0) != 1) continue;

        // =====================================================================
        // DELAY MÍNIMO — o evento do watcher só DESPERTA o dispatcher; o
        // intervalo configurado é sempre respeitado.
        //
        // Sem isso, o tracker de monstros (que pisca: detectado -> removido ->
        // detectado no mesmo ciclo de 150ms) sinalizava o evento várias vezes
        // por segundo e o dispatcher disparava uma thread de ataque por
        // monstro a cada sinal: milhares de skill packets por minuto. O log
        // virava dezenas de MB (com fflush por linha) e o CLIENT TRAVAVA
        // esperando o lock global do logger.
        // =====================================================================
        const DWORD now = GetTickCount();
        if (lastAttackTick != 0) {
            const DWORD elapsed = now - lastAttackTick;
            if (elapsed < (DWORD)g_SessionDelayMs) {
                Sleep((DWORD)g_SessionDelayMs - elapsed);
            }
        }

        int hitCount = g_SessionHitCount;
        if (hitCount < 1) hitCount = 1;

        bool didAttack = false;

        // ---- Modo: atacar alvos do map (monstros/outros + jogadores se "atacar todos") ----
        // O map contém os tipos selecionados. Jogadores só são atacados pelo map
        // se "Atacar todos" estiver marcado; caso contrário, são atacados só pelo clique.
        {
            EnterCriticalSection(&g_MapLock);
            int targetCount = 0;
            uint16_t* targets = nullptr;
            if (!g_MonsterMap.empty()) {
                // Conta quantos alvos serão atacados (exclui jogadores se só "no clique")
                for (auto& pair : g_MonsterMap) {
                    if (pair.second == ObjType::OBJ_PLAYER && !g_SessionAttackPlayersAll) continue;
                    targetCount++;
                }
                if (targetCount > 0) {
                    targets = new uint16_t[targetCount];
                    int idx = 0;
                    for (auto& pair : g_MonsterMap) {
                        if (pair.second == ObjType::OBJ_PLAYER && !g_SessionAttackPlayersAll) continue;
                        targets[idx++] = pair.first;
                    }
                }
            }
            LeaveCriticalSection(&g_MapLock);

            if (targetCount > 0) {
                didAttack = true;

                // Resumo a cada 5s (visibilidade sem despejar o log).
                static DWORD lastSummaryTick = 0;
                static long roundCount = 0;
                roundCount++;
                if (lastSummaryTick == 0) lastSummaryTick = GetTickCount();
                else if (now - lastSummaryTick >= 5000) {
                    const long rounds = roundCount;
                    lastSummaryTick = now;
                    roundCount = 0;
                    Logger::Log("ATTACK", "Dispatcher: %ld rodadas em 5s (%d alvo(s), delay=%dms)",
                                rounds, targetCount, g_SessionDelayMs);
                }

                // Fire-and-forget: uma thread por alvo (sem log por ciclo — evita poluir o log)
                for (int t = 0; t < targetCount; t++) {
                    auto* p = new AttackThreadParam{ targets[t], hitCount };
                    HANDLE h = CreateThread(NULL, 0, AttackSingleMobThread, p, 0, NULL);
                    if (h) {
                        CloseHandle(h);
                    } else {
                        delete p;
                    }
                }
            }

            delete[] targets;
        }

        // ---- Modo: atacar jogador no clique (até ele sumir) ----
        // O jogo grava o slot do alvo clicado em 0x081C03A0 (TargetSlotIndex, -1 = nenhum)
        // e o entity index em 0x081C03A4 (TargetEntityIndex, usado em skill packets).
        // Usamos o slot (acesso direto ao array) com fallback para busca por entity index.
        // Só ataca se o alvo clicado for um jogador (type 1) e estiver vivo.
        if (g_SessionAttackPlayersClick) {
            int32_t clickedSlot = GameMemory::Globals::TargetSlotIndex();
            uint16_t clickedIdx = GameMemory::Globals::TargetEntityIndex();

            // Diagnóstico temporário: loga os valores a cada ~2s
            {
                static DWORD lastClickLog = 0;
                DWORD nowTick = GetTickCount();
                if (nowTick - lastClickLog >= 2000) {
                    lastClickLog = nowTick;
                    uintptr_t dbgObj = 0;
                    if (clickedSlot >= 0 && clickedSlot < GameAddresses::MAX_OBJECTS) {
                        dbgObj = GameMemory::Object::ByIndex(clickedSlot);
                    } else if (clickedIdx != 0) {
                        dbgObj = GameMemory::Object::FindByEntityIndex(clickedIdx);
                    }
                    if (dbgObj) {
                        BYTE dbgType = *(BYTE*)(dbgObj + GameObject::OFF_TYPE);
                        BYTE dbgLive = *(BYTE*)(dbgObj + GameObject::OFF_ISLIVE);
                        uint16_t dbgIdx = *(uint16_t*)(dbgObj + GameObject::OFF_INDEX);
                        Logger::Log("ATTACK", "ClickDiag: slot=%d entityIdx=0x%04X objIdx=0x%04X type=%d live=%d",
                                    clickedSlot, clickedIdx, dbgIdx, dbgType, dbgLive);
                    } else {
                        Logger::Log("ATTACK", "ClickDiag: slot=%d entityIdx=0x%04X obj=NULL", clickedSlot, clickedIdx);
                    }
                }
            }

            // Resolve o objeto alvo: tenta pelo slot primeiro, senão pelo entity index
            uintptr_t targetObj = 0;
            if (clickedSlot >= 0 && clickedSlot < GameAddresses::MAX_OBJECTS) {
                targetObj = GameMemory::Object::ByIndex(clickedSlot);
            }
            if (!targetObj && clickedIdx != 0) {
                targetObj = GameMemory::Object::FindByEntityIndex(clickedIdx);
            }

            if (targetObj) {
                BYTE type = *(BYTE*)(targetObj + GameObject::OFF_TYPE);
                BYTE live = *(BYTE*)(targetObj + GameObject::OFF_ISLIVE);
                if (type == ObjType::OBJ_PLAYER && live) {
                    // Usa o entity index do objeto (não o do global) para o pacote
                    uint16_t targetEntityIdx = *(uint16_t*)(targetObj + GameObject::OFF_INDEX);
                    bool inRange = true;
                    if (g_SessionRange > 0.0f) {
                        uintptr_t localPlayer = GameMemory::Player::Object();
                        if (localPlayer) {
                            float lx = *(float*)(localPlayer + GameObject::OFF_X);
                            float ly = *(float*)(localPlayer + GameObject::OFF_Y);
                            float tx = *(float*)(targetObj + GameObject::OFF_X);
                            float ty = *(float*)(targetObj + GameObject::OFF_Y);
                            float dx = tx - lx, dy = ty - ly;
                            float dist = sqrtf(dx * dx + dy * dy);
                            inRange = (dist <= g_SessionRange * TILE_SIZE);
                        }
                    }
                    if (inRange) {
                        didAttack = true;
                        for (int h = 0; h < hitCount; h++) {
                            if (g_SessionType == (uint8_t)MuOpcodes::AttackType::Skill) {
                                SendSkillAttack(targetEntityIdx, g_SessionSkillId);
                            } else {
                                SendAttack(targetEntityIdx);
                            }
                        }
                    }
                }
            }
        }

        if (didAttack) {
            lastAttackTick = GetTickCount();
        }
    }

    Logger::Log("ATTACK", "Dispatcher encerrado");
    return 0;
}

// ---- Parse de key=value: helpers compartilhados em CommandParams.h ----

// ============================================================================
// Handlers de mensagens tipadas (registrados via REGISTER_MESSAGE)
// ============================================================================

static void HandleAttackStart(const char* payload) {
    // payload: "delay=500|range=8|type=0|hitcount=3|skill=1E|attackmonsters=1|attackplayerall=0|attackplayerclick=0|attackothers=0"
    g_SessionDelayMs = g_Config.defaultDelayMs;
    g_SessionRange   = g_Config.defaultRange;
    g_SessionType    = g_Config.defaultType;
    g_SessionSkillId = g_Config.defaultSkillId;
    g_SessionHitCount = g_Config.defaultHitCount;
    g_SessionAttackMonsters    = g_Config.defaultAttackMonsters ? 1 : 0;
    g_SessionAttackPlayersAll  = g_Config.defaultAttackPlayersAll ? 1 : 0;
    g_SessionAttackPlayersClick = g_Config.defaultAttackPlayersClick ? 1 : 0;
    g_SessionAttackOthers      = g_Config.defaultAttackOthers ? 1 : 0;

    if (payload && *payload) {
        int ival; float fval; uint16_t hval;
        if (CommandParams::ParseIntParam(payload, "delay", &ival) && ival > 0) g_SessionDelayMs = ival;
        if (CommandParams::ParseFloatParam(payload, "range", &fval) && fval > 0.0f) g_SessionRange = fval;
        if (CommandParams::ParseHexParam(payload, "skill", &hval)) {
            g_SessionSkillId = hval;
            if (g_SessionType == 0) g_SessionType = (uint8_t)MuOpcodes::AttackType::Skill;
        }
        CommandParams::ParseByteParam(payload, "type", &g_SessionType);
        if (CommandParams::ParseIntParam(payload, "hitcount", &ival) && ival >= 1) g_SessionHitCount = ival;
        if (CommandParams::ParseIntParam(payload, "attackmonsters", &ival)) g_SessionAttackMonsters = (ival != 0) ? 1 : 0;
        if (CommandParams::ParseIntParam(payload, "attackplayerall", &ival)) g_SessionAttackPlayersAll = (ival != 0) ? 1 : 0;
        if (CommandParams::ParseIntParam(payload, "attackplayerclick", &ival)) g_SessionAttackPlayersClick = (ival != 0) ? 1 : 0;
        if (CommandParams::ParseIntParam(payload, "attackothers", &ival)) g_SessionAttackOthers = (ival != 0) ? 1 : 0;
    }

    InterlockedExchange(&g_Attacking, 1);
    Logger::Log("ATTACK", "START auto-attack delay=%dms range=%.1f type=%s skill=0x%04X hitcount=%d monsters=%d playersAll=%d playersClick=%d others=%d",
                g_SessionDelayMs, g_SessionRange,
                g_SessionType ? "SKILL" : "NORMAL", g_SessionSkillId, g_SessionHitCount,
                (int)g_SessionAttackMonsters, (int)g_SessionAttackPlayersAll,
                (int)g_SessionAttackPlayersClick, (int)g_SessionAttackOthers);
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
        if (g_SessionType == 0) g_SessionType = (uint8_t)MuOpcodes::AttackType::Skill;
    }
    if (CommandParams::ParseIntParam(payload, "hitcount", &ival) && ival >= 1) {
        g_Config.defaultHitCount = ival;
        g_SessionHitCount = ival;
    }
    if (CommandParams::ParseIntParam(payload, "attackmonsters", &ival)) {
        g_Config.defaultAttackMonsters = (ival != 0);
        g_SessionAttackMonsters = (ival != 0) ? 1 : 0;
    }
    if (CommandParams::ParseIntParam(payload, "attackplayerall", &ival)) {
        g_Config.defaultAttackPlayersAll = (ival != 0);
        g_SessionAttackPlayersAll = (ival != 0) ? 1 : 0;
    }
    if (CommandParams::ParseIntParam(payload, "attackplayerclick", &ival)) {
        g_Config.defaultAttackPlayersClick = (ival != 0);
        g_SessionAttackPlayersClick = (ival != 0) ? 1 : 0;
    }
    if (CommandParams::ParseIntParam(payload, "attackothers", &ival)) {
        g_Config.defaultAttackOthers = (ival != 0);
        g_SessionAttackOthers = (ival != 0) ? 1 : 0;
    }
    Logger::Log("ATTACK", "CONFIG atualizado: delay=%dms range=%.1f type=%s skill=0x%04X hitcount=%d monsters=%d playersAll=%d playersClick=%d others=%d",
                g_SessionDelayMs, g_SessionRange,
                g_SessionType ? "SKILL" : "NORMAL", g_SessionSkillId, g_SessionHitCount,
                (int)g_SessionAttackMonsters, (int)g_SessionAttackPlayersAll,
                (int)g_SessionAttackPlayersClick, (int)g_SessionAttackOthers);
}
REGISTER_MESSAGE(ATTACK_CONFIG, HandleAttackConfig);

static void HandleAttackPlayerIdx(const char* payload) {
    uint16_t playerIdx = 0;
    if (CommandParams::ParseHexParam(payload, "idx", &playerIdx) && playerIdx != 0) {
        GameMemory::Player::SetIndex(playerIdx);
        Logger::Log("ATTACK", "PLAYERIDX setado: 0x%04X", playerIdx);
    } else {
        unsigned int val = 0;
        if (sscanf_s(payload, "%x", &val) == 1 && val != 0) {
            GameMemory::Player::SetIndex((uint16_t)val);
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

    Logger::Log("ATTACK", "Inicializado (default: delay=%dms range=%.1f type=%s hitcount=%d monsters=%d playersAll=%d playersClick=%d others=%d)",
                g_Config.defaultDelayMs, g_Config.defaultRange,
                g_Config.defaultType ? "SKILL" : "NORMAL", g_Config.defaultHitCount,
                (int)g_Config.defaultAttackMonsters, (int)g_Config.defaultAttackPlayersAll,
                (int)g_Config.defaultAttackPlayersClick, (int)g_Config.defaultAttackOthers);
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