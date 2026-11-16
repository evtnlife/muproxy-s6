// potion/AutoPotion.cpp — Bebida automática de potion (HP/MP)
//
// O módulo lê o life/mana do jogador local em memória e injeta o pacote de uso
// de item (C1 06 26) quando o percentual cai abaixo do limiar.
//
// "Learn & replay": o slot do inventário é DESCOBERTO observing o pacote que o
// próprio jogador manda ao beber a potion (o sniffer entrega o C1 06 26 ainda
// na forma Camada-2; aqui ele é decodificado e guardado como modelo). O life/mana
// antes e depois dizem se aquele modelo é de HP ou de MP.
//
// Detalhes do protocolo (servidor de referência eMU GameServer):
//   PMSG_ITEM_USE_RECV: PBMSG_HEAD + SourceSlot + TargetSlot + type
//   CGItemUseRecv → CharacterUsePotion (HP%/MP%/SD% do MaxLife/MaxMana)
//   anti-hack: 2º uso dentro de CheckAutoPotionHackTolerance ms é rejeitado
#include "AutoPotion.h"
#include "MuOpcodes.h"
#include "MuCipher.h"
#include "PacketInjector.h"
#include "GameMemory/Player.h"
#include "MessageRegistry.h"
#include "CommandParams.h"
#include "Config.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>

namespace AutoPotion {

// ---- Parâmetros do watcher ----
static constexpr int TICK_MS            = 100;  // periodicidade da leitura de life/mana
static constexpr int PENDING_TIMEOUT_MS = 400;  // espera o efeito chegar p/ classificar o modelo

// ---- Estado global ----
static HANDLE g_hThread = nullptr;
static volatile LONG g_Running = 0;   // thread viva
static volatile LONG g_Active  = 0;   // auto-potion ligado (POTION_START)

// ::Config = include/Config.h (o namespace global); ::Settings = o struct deste módulo.
static Settings g_Config = {
    ::Config::AUTO_POTION_DEFAULT_HP_THRESHOLD,
    ::Config::AUTO_POTION_DEFAULT_MP_THRESHOLD,
    ::Config::AUTO_POTION_DEFAULT_INTERVAL_MS,
    -1,   // hpSlot: usa o modelo aprendido
    -1    // mpSlot: usa o modelo aprendido
};
static CRITICAL_SECTION g_Lock;

// Modelos aprendidos (pacote C1 06 26 já DECODIFICADO — 6 bytes)
static uint8_t g_hpModel[6] = { 0 };
static uint8_t g_mpModel[6] = { 0 };
static bool g_hpModelValid = false;
static bool g_mpModelValid = false;

// Uso observado pelo cliente (aguardando o efeito para classificar)
static bool     g_pendingValid = false;
static uint8_t  g_pendingModel[6] = { 0 };
static uint16_t g_pendingLife = 0;
static uint16_t g_pendingMana = 0;
static DWORD    g_pendingTick = 0;

// Último uso injetado (controle do intervalo / anti-hack)
static DWORD g_lastUseTick = 0;

// ============================================================================
// Monta o pacote de uso de item (6 bytes, plaintext).
//   isHp = true  → potion de HP
//   isHp = false → potion de MP
// Retorna false se não há modelo (e nem slot configurado).
// ============================================================================
static bool BuildItemUsePacket(bool isHp, uint8_t* outPacket)
{
    int slot = isHp ? g_Config.hpSlot : g_Config.mpSlot;

    // Slot configurado à mão: monta o pacote (TargetSlot 0xFF e type 0 — o
    // servidor só usa TargetSlot para Fruit e rejeita TargetSlot == SourceSlot).
    if (slot >= 0) {
        outPacket[0] = (uint8_t)MuOpcodes::HeaderType::C1;
        outPacket[1] = MuOpcodes::Size::ItemUse;
        outPacket[2] = (uint8_t)MuOpcodes::ClientToServer::ItemUse;
        outPacket[3] = (uint8_t)slot;
        outPacket[4] = 0xFF;
        outPacket[5] = 0x00;
        return true;
    }

    // Modelo aprendido: replay verbatim (o payload é o que o cliente mandou).
    EnterCriticalSection(&g_Lock);
    const uint8_t* model = isHp ? g_hpModel : g_mpModel;
    bool valid = isHp ? g_hpModelValid : g_mpModelValid;
    if (valid) memcpy(outPacket, model, 6);
    LeaveCriticalSection(&g_Lock);

    return valid;
}

// ============================================================================
// Dispara o uso de uma potion (respeita o intervalo mínimo / anti-hack).
// ============================================================================
static bool UsePotion(bool isHp, int percent)
{
    DWORD now = GetTickCount();
    if (now - g_lastUseTick < (DWORD)g_Config.intervalMs) {
        return false;   // ainda dentro do intervalo mínimo
    }

    uint8_t packet[6];
    if (!BuildItemUsePacket(isHp, packet)) {
        return false;   // sem modelo aprendido e sem slot configurado
    }

    if (!PacketInjector::Inject(packet, 6)) {
        Logger::Log("POTION", "falha ao injetar o uso de item (slot=%u)", packet[3]);
        return false;
    }

    g_lastUseTick = now;
    Logger::Log("POTION", "%s disparada em %d%% (slot=%u) — uso injetado",
                isHp ? "potion de HP" : "potion de MP", percent, packet[3]);
    return true;
}

// ============================================================================
// Resolve o modelo observado: se o life subiu, o modelo é de HP; se o mana
// subiu, é de MP. (Chamado pelo watcher, fora da thread do hook.)
// ============================================================================
static void ResolvePendingModel()
{
    if (!g_pendingValid) return;

    DWORD now = GetTickCount();
    if (now - g_pendingTick < (DWORD)PENDING_TIMEOUT_MS) return;   // efeito ainda não chegou

    uint16_t life = GameMemory::Player::Life();
    uint16_t mana = GameMemory::Player::Mana();

    EnterCriticalSection(&g_Lock);
    if (life > g_pendingLife) {
        memcpy(g_hpModel, g_pendingModel, 6);
        g_hpModelValid = true;
        Logger::Log("POTION", "modelo de HP aprendido (slot=%u)", g_hpModel[3]);
    } else if (mana > g_pendingMana) {
        memcpy(g_mpModel, g_pendingModel, 6);
        g_mpModelValid = true;
        Logger::Log("POTION", "modelo de MP aprendido (slot=%u)", g_mpModel[3]);
    } else {
        Logger::Log("POTION", "uso observado sem efeito em life/mana (slot=%u) — não classificado",
                    g_pendingModel[3]);
    }
    g_pendingValid = false;
    LeaveCriticalSection(&g_Lock);
}

// ============================================================================
// Verifica os limiares e dispara a potion correspondente.
// ============================================================================
static void CheckThresholds()
{
    uint16_t life    = GameMemory::Player::Life();
    uint16_t maxLife = GameMemory::Player::MaxLife();
    uint16_t mana    = GameMemory::Player::Mana();
    uint16_t maxMana = GameMemory::Player::MaxMana();

    if (!maxLife || !maxMana) return;   // sem player object (ainda nao logado)
    if (life == 0) return;               // morto: o servidor ignora (DieRegen)

    int hpPercent = (int)((life * 100) / maxLife);
    int mpPercent = (int)((mana * 100) / maxMana);

    if (hpPercent < g_Config.hpThreshold) {
        if (UsePotion(true, hpPercent)) return;   // uma potion por vez
    }
    if (mpPercent < g_Config.mpThreshold) {
        UsePotion(false, mpPercent);
    }
}

// ============================================================================
// Watcher thread: lê life/mana e dispara as potions.
// ============================================================================
static DWORD WINAPI WatcherThread(LPVOID)
{
    Logger::Log("POTION", "Watcher iniciado (tick=%dms)", TICK_MS);

    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        if (InterlockedCompareExchange(&g_Active, 1, 1) == 1) {
            ResolvePendingModel();
            CheckThresholds();
        }
        Sleep(TICK_MS);
    }

    Logger::Log("POTION", "Watcher encerrado");
    return 0;
}

// ============================================================================
// Handlers de mensagens tipadas (MessageRegistry)
// ============================================================================
static void HandlePotionStart(const char* payload)
{
    (void)payload;
    InterlockedExchange(&g_Active, 1);
    Logger::Log("POTION", "auto-potion LIGADO (hp<%d%% mp<%d%% intervalo=%dms)",
                g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs);
    LogStatus();
}
REGISTER_MESSAGE(POTION_START, HandlePotionStart);

static void HandlePotionStop(const char* payload)
{
    (void)payload;
    InterlockedExchange(&g_Active, 0);
    Logger::Log("POTION", "auto-potion DESLIGADO");
}
REGISTER_MESSAGE(POTION_STOP, HandlePotionStop);

static void HandlePotionConfig(const char* payload)
{
    if (!payload || !*payload) return;

    int ival = 0;
    if (CommandParams::ParseIntParam(payload, "hp", &ival) && ival >= 0 && ival <= 100)
        g_Config.hpThreshold = ival;
    if (CommandParams::ParseIntParam(payload, "mp", &ival) && ival >= 0 && ival <= 100)
        g_Config.mpThreshold = ival;
    if (CommandParams::ParseIntParam(payload, "interval", &ival) && ival >= 100)
        g_Config.intervalMs = ival;
    if (CommandParams::ParseIntParam(payload, "hpslot", &ival) && ival >= 0 && ival < 256)
        g_Config.hpSlot = ival;
    if (CommandParams::ParseIntParam(payload, "mpslot", &ival) && ival >= 0 && ival < 256)
        g_Config.mpSlot = ival;

    Logger::Log("POTION", "config: hp<%d%% mp<%d%% intervalo=%dms hpslot=%d mpslot=%d",
                g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs,
                g_Config.hpSlot, g_Config.mpSlot);
}
REGISTER_MESSAGE(POTION_CONFIG, HandlePotionConfig);

static void HandlePotionStatus(const char* payload)
{
    (void)payload;
    LogStatus();
}
REGISTER_MESSAGE(POTION_STATUS, HandlePotionStatus);

// ============================================================================
// API pública
// ============================================================================
bool Initialize()
{
    InitializeCriticalSection(&g_Lock);
    g_Running = 1;

    g_hThread = CreateThread(NULL, 0, WatcherThread, NULL, 0, NULL);
    if (!g_hThread) {
        Logger::Log("POTION", "FALHA ao criar a thread do watcher");
        g_Running = 0;
        DeleteCriticalSection(&g_Lock);
        return false;
    }

    Logger::Log("POTION", "Inicializado (hp<%d%% mp<%d%% intervalo=%dms)",
                g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs);
    return true;
}

void Shutdown()
{
    InterlockedExchange(&g_Active, 0);
    InterlockedExchange(&g_Running, 0);

    if (g_hThread) {
        WaitForSingleObject(g_hThread, 3000);
        CloseHandle(g_hThread);
        g_hThread = nullptr;
    }
    DeleteCriticalSection(&g_Lock);

    Logger::Log("POTION", "Desligado");
}

void Start()
{
    InterlockedExchange(&g_Active, 1);
}

void Stop()
{
    InterlockedExchange(&g_Active, 0);
}

bool IsRunning()
{
    return InterlockedCompareExchange(&g_Active, 1, 1) == 1;
}

const Settings& GetConfig()
{
    return g_Config;
}

void SetConfig(const Settings& cfg)
{
    g_Config = cfg;
}

void OnItemUsePacket(const uint8_t* data, int len)
{
    if (!data || len != 6) return;
    if (data[0] != (uint8_t)MuOpcodes::HeaderType::C1) return;
    if (data[2] != (uint8_t)MuOpcodes::ClientToServer::ItemUse) return;

    // O sniffer só desfez a Layer-1: o payload ainda está na forma Camada-2.
    uint8_t plain[6];
    memcpy(plain, data, 6);
    MuCipher::UnXorData((char*)plain, 3, 6);

    EnterCriticalSection(&g_Lock);
    memcpy(g_pendingModel, plain, 6);
    g_pendingLife = GameMemory::Player::Life();
    g_pendingMana = GameMemory::Player::Mana();
    g_pendingTick = GetTickCount();
    g_pendingValid = true;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "uso de item observado (cliente): slot=%u target=0x%02X type=0x%02X",
                plain[3], plain[4], plain[5]);
}

void LogStatus()
{
    uint16_t life    = GameMemory::Player::Life();
    uint16_t maxLife = GameMemory::Player::MaxLife();
    uint16_t mana    = GameMemory::Player::Mana();
    uint16_t maxMana = GameMemory::Player::MaxMana();

    EnterCriticalSection(&g_Lock);
    bool hpValid = g_hpModelValid;
    bool mpValid = g_mpModelValid;
    unsigned hpSlot = hpValid ? g_hpModel[3] : 0;
    unsigned mpSlot = mpValid ? g_mpModel[3] : 0;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "estado: %s | hp<%d%% mp<%d%% intervalo=%dms | modelo HP=%s (slot %u) | modelo MP=%s (slot %u) | life=%u/%u mana=%u/%u",
                IsRunning() ? "LIGADO" : "DESLIGADO",
                g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs,
                hpValid ? "ok" : "-", hpSlot, mpValid ? "ok" : "-", mpSlot,
                (unsigned)life, (unsigned)maxLife, (unsigned)mana, (unsigned)maxMana);

    if (g_Config.hpSlot >= 0) Logger::Log("POTION", "  slot manual de HP: %d", g_Config.hpSlot);
    if (g_Config.mpSlot >= 0) Logger::Log("POTION", "  slot manual de MP: %d", g_Config.mpSlot);
}

} // namespace AutoPotion
