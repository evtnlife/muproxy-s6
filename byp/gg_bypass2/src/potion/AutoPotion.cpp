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
#include "SimpleModulusCipher.h"
#include "PacketInjector.h"
#include "PacketDiag.h"
#include "CommandChannel.h"
#include "GameMemory/Player.h"
#include "GameMemory/ClientVitals.h"
#include "MessageRegistry.h"
#include "CommandParams.h"
#include "Config.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>

namespace AutoPotion {

// ---- Parâmetros do watcher ----
static constexpr int TICK_MS            = 100;  // periodicidade da leitura de life/mana
static constexpr int PENDING_TIMEOUT_MS = 3000; // espera o efeito chegar p/ classificar o modelo

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

// Inventário do personagem (pacote F3 10 do servidor): slot → ID do item.
// Permite configurar a potion pelo ID do item em vez do slot.
#define INVENTORY_SLOTS 256
static int g_slotItemId[INVENTORY_SLOTS];     // -1 = sem item
static int g_itemListCount = 0;

// ---------------------------------------------------------------------------
// Potions reconhecidas pelo ID do item.
//
// O ID do item é GET_ITEM(grupo,idx) = grupo*512 + idx (MAX_ITEM_TYPE=512).
// A lista abaixo vem do servidor (eMU `CObjectManager::CharacterUsePotion` +
// `CItemManager::CGItemUseRecv`) cruzada com a tabela de itens do cliente
// (Data\Local\Eng\ItemTooltip_eng.txt):
//
//   14,0        Apple                    HP
//   14,1..3     Small/Middle/Large Healing   HP
//   14,4..6     Small/Middle/Large Mana      MP
//   14,38..40   Small/Middle/Large Complex   HP + SD
//   14,70/71    Elite Healing / Mana         HP / MP
//   13,117..120 "Vitality Potion" (exclusivo deste server)   HP
//
// ⚠️ As potions NÃO são adivinhadas: o ID vem da lista de inventário que o
// servidor manda (F3 10 no login / F3 14 a cada mudança). O nome abaixo é
// só para a UI mostrar o que está sendo usado.
// ---------------------------------------------------------------------------
struct PotionDef {
    unsigned    id;
    bool        isHp;
    const char* name;
};

static const PotionDef POTIONS[] = {
    { 7168, true,  "Apple" },                    // 14,0
    { 7169, true,  "Small Healing Potion" },     // 14,1
    { 7170, true,  "Healing Potion" },           // 14,2
    { 7171, true,  "Large Healing Potion" },     // 14,3
    { 7172, false, "Small Mana Potion" },        // 14,4
    { 7173, false, "Mana Potion" },              // 14,5
    { 7174, false, "Large Mana Potion" },        // 14,6
    { 7206, true,  "Small Complex Potion" },     // 14,38
    { 7207, true,  "Complex Potion" },           // 14,39
    { 7208, true,  "Large Complex Potion" },      // 14,40
    { 7238, true,  "Elite Healing Potion" },     // 14,70
    { 7239, false, "Elite Mana Potion" },        // 14,71
    { 6773, true,  "Very Weak Vitality Potion" },   // 13,117
    { 6774, true,  "Weak Vitality Potion" },         // 13,118
    { 6775, true,  "Mediocre Vitality Potion" },     // 13,119
    { 6776, true,  "Excellent Vitality Potion" },    // 13,120
};
static const int POTION_COUNT = (int)(sizeof(POTIONS) / sizeof(POTIONS[0]));

static int g_hpSlots[INVENTORY_SLOTS];  static int g_hpSlotCount = 0;
static int g_mpSlots[INVENTORY_SLOTS];  static int g_mpSlotCount = 0;
static int g_hpSlotIdx = 0;             // slot de HP em uso (round-robin)
static int g_mpSlotIdx = 0;

// Verificação do uso automático: se o life/mana não sobe depois do uso, o
// slot está vazio (ou o servidor recusou) → troca para o próximo slot.
static bool     g_autoUseValid = false;
static bool     g_autoUseIsHp = false;
static int      g_autoUseSlot = -1;
static unsigned g_autoUseLifeBefore = 0, g_autoUseManaBefore = 0;
static DWORD    g_autoUseTick = 0;

static const PotionDef* FindPotionDef(unsigned id)
{
    for (int i = 0; i < POTION_COUNT; i++) {
        if (POTIONS[i].id == id) return &POTIONS[i];
    }
    return nullptr;
}

static bool IsHpPotionId(unsigned id)
{
    const PotionDef* p = FindPotionDef(id);
    return p != nullptr && p->isHp;
}

static bool IsMpPotionId(unsigned id)
{
    const PotionDef* p = FindPotionDef(id);
    return p != nullptr && !p->isHp;
}

// Nome do item ("Healing Potion", ...) — "" se não for potion.
static const char* PotionName(unsigned id)
{
    const PotionDef* p = FindPotionDef(id);
    return p ? p->name : "";
}

// Reconstrói a lista de slots de potion a partir do inventário.
static void RebuildPotionSlots()
{
    g_hpSlotCount = 0;
    g_mpSlotCount = 0;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        const int id = g_slotItemId[i];
        if (id < 0) continue;
        if (IsHpPotionId((unsigned)id))       g_hpSlots[g_hpSlotCount++] = i;
        else if (IsMpPotionId((unsigned)id))  g_mpSlots[g_mpSlotCount++] = i;
    }
    if (g_hpSlotIdx >= g_hpSlotCount) g_hpSlotIdx = 0;
    if (g_mpSlotIdx >= g_mpSlotCount) g_mpSlotIdx = 0;

    Logger::Log("POTION", "pocoes no inventario: %d slot(s) de HP (%s), %d de MP (%s)%s",
                g_hpSlotCount,
                g_hpSlotCount ? PotionName((unsigned)g_slotItemId[g_hpSlots[0]]) : "-",
                g_mpSlotCount,
                g_mpSlotCount ? PotionName((unsigned)g_slotItemId[g_mpSlots[0]]) : "-",
                (g_hpSlotCount || g_mpSlotCount) ? "" : " (nenhuma — nem no inventario)");
}

// Slot de HP/MP atualmente em uso (-1 = nenhum).
static int CurrentHpSlot() { return g_hpSlotCount ? g_hpSlots[g_hpSlotIdx % g_hpSlotCount] : -1; }
static int CurrentMpSlot() { return g_mpSlotCount ? g_mpSlots[g_mpSlotIdx % g_mpSlotCount] : -1; }

// Uso observado pelo cliente (aguardando o efeito para classificar)
static bool     g_pendingValid = false;
static uint8_t  g_pendingModel[6] = { 0 };
static unsigned g_pendingLife = 0;      // valor ANTES do uso
static unsigned g_pendingMana = 0;
static unsigned g_pendingPeakLife = 0;  // MÁXIMO visto durante a janela
static unsigned g_pendingPeakMana = 0;
static DWORD    g_pendingTick = 0;

// Último uso injetado (controle do intervalo / anti-hack)
static DWORD g_lastUseTick = 0;

// ============================================================================
// Fontes de vitais (S→C). Cada uma guarda o tick de CHEGADA: o módulo usa
// sempre o valor MAIS RECENTE, nunca um snapshot velho.
//
//  1) C1 26 / C1 27 (update de life/mana). O byte "type" diz o que o valor
//     significa (servidor eMU, GCLifeSend/GCManaSend):
//        0xFF → vida/mana ATUAL
//        0xFE → vida/mana MÁXIMA   ⚠️ NÃO é a vida atual!
//        0xFD → vida atual (truncada em BYTE pelo servidor)
//     Tratar o 0xFE como "vida atual" prende a vida no máximo e o auto-potion
//     nunca mais dispara — foi exatamente o bug do "parou de funcionar":
//     o log mostrava "trafego: life=214" (o máximo) com o personagem a 4 HP.
//  2) F3 E1 (character calc): snapshot do servidor com ATUAL + MÁXIMO.
// ============================================================================
static unsigned g_netLife = 0, g_netMana = 0;
static DWORD    g_netLifeTick = 0, g_netManaTick = 0;
static bool     g_netLifeValid = false, g_netManaValid = false;

// Máximos vistos em pacotes type 0xFE (o servidor manda o max do personagem).
static unsigned g_netMaxLife = 0, g_netMaxMana = 0;

// Snapshot do servidor (F3 E1 / F3 E0): atual + máximo + tick de chegada.
static unsigned g_charLife = 0, g_charMaxLife = 0;
static unsigned g_charMana = 0, g_charMaxMana = 0;
static DWORD    g_charTick = 0;
static bool     g_charStatsValid = false;

// Motivo da última vez que o limiar foi atingido e a potion NÃO saiu
// (mostrado no log para o usuário não ficar no escuro).
enum class BlockReason { None, NoModel, Interval, NoVitals, InjectFailed };
static BlockReason g_lastBlock = BlockReason::None;
static int         g_blockCount = 0;

// Histórico curto do life/mana/BP/escudo vindos da rede. Serve como
// ASSINATURA para localizar os globals da HUD no main.exe (ViewCurHP/
// ViewMaxHP/ViewCurMP/ViewMaxMP/ViewCurBP/ViewCurSD): o cliente aplica o
// pacote um passo atrás do sniffer, então qualquer valor recente serve.
static GameMemory::Player::ClientVitalsHints g_hints;

static void PushHint(unsigned* hints, int& count, int cap, unsigned value)
{
    for (int i = cap - 1; i > 0; i--) hints[i] = hints[i - 1];
    hints[0] = value;
    if (count < cap) count++;
}

// ============================================================================
// Vitals — life/mana do jogador local.
//
// Fontes (nenhuma delas pode derrubar o jogo — todas com SEH):
//   1) ObjectCharacter em memória (life/mana/maxLife/maxMana)
//   2) PartyList slot 0 (o jogador local): life/maxLife
//   3) Tráfego do servidor (C1 26 / C1 27): valor ATUAL
// ============================================================================
struct Vitals {
    unsigned life = 0, maxLife = 0, mana = 0, maxMana = 0;
    int lifePct = -1;                       // % direto da party list (-1 = sem)
    bool haveLife = false, haveMana = false;
    const char* source = "nenhum";
};

static Vitals ReadVitals()
{
    Vitals v;

    // ---- (1) CLIENTE: os globals da HUD do main.exe ----
    // ViewCurHP / ViewMaxHP / ViewCurMP / ViewMaxMP. É a fonte mais completa
    // (traz o MÁXIMO, que o servidor BlackRock não manda em nenhum pacote)
    // e não tem o atraso da rede.
    unsigned cLife = 0, cMaxLife = 0, cMana = 0, cMaxMana = 0;
    if (GameMemory::Player::ReadClientVitals(cLife, cMaxLife, cMana, cMaxMana)) {
        v.life = cLife;  v.maxLife = cMaxLife;
        v.mana = cMana;  v.maxMana = cMaxMana;
        v.haveLife = true;
        v.haveMana = true;
        v.source = "cliente";
        int pct = GameMemory::Player::PartyLifePercent(0);
        if (pct >= 0) v.lifePct = pct;
        return v;
    }

    // ---- (2) SERVIDOR — character calc (F3 E1 / F3 E0): atual + máximo ----
    unsigned maxLife = g_charStatsValid ? g_charMaxLife : 0;
    unsigned maxMana = g_charStatsValid ? g_charMaxMana : 0;
    if (g_netMaxLife > maxLife) maxLife = g_netMaxLife;
    if (g_netMaxMana > maxMana) maxMana = g_netMaxMana;

    // ---- ATUAL: o valor mais RECENTE entre o calc e o update de tráfego ----
    unsigned life = 0, mana = 0;
    DWORD lifeTick = 0, manaTick = 0;
    bool haveLife = false, haveMana = false;

    if (g_charStatsValid) {
        life = g_charLife; lifeTick = g_charTick; haveLife = true;
        mana = g_charMana; manaTick = g_charTick; haveMana = true;
    }
    if (g_netLifeValid && (!haveLife || g_netLifeTick >= lifeTick)) {
        life = g_netLife; lifeTick = g_netLifeTick; haveLife = true;
    }
    if (g_netManaValid && (!haveMana || g_netManaTick >= manaTick)) {
        mana = g_netMana; manaTick = g_netManaTick; haveMana = true;
    }

    // ---- FALLBACK: memória do jogo (ObjectCharacter / PartyList) ----
    if (!haveLife && GameMemory::Player::UserObject()) {
        life    = GameMemory::Player::Life();
        maxLife = GameMemory::Player::MaxLife();
        haveLife = (maxLife > 0);
        if (v.source[0] == 'n') v.source = "memoria";
    }
    if (!haveMana && GameMemory::Player::UserObject()) {
        mana    = GameMemory::Player::Mana();
        maxMana = GameMemory::Player::MaxMana();
        haveMana = (maxMana > 0);
    }
    if (!haveLife) {
        int pmax = GameMemory::Player::PartyMaxLife(0);
        if (pmax > 0) {
            life = (unsigned)GameMemory::Player::PartyLife(0);
            maxLife = (unsigned)pmax;
            haveLife = true;
            if (v.source[0] == 'n') v.source = "party-list";
        }
    }

    v.life = life;  v.maxLife = maxLife;
    v.mana = mana;  v.maxMana = maxMana;
    v.haveLife = haveLife && maxLife > 0;
    v.haveMana = haveMana && maxMana > 0;
    if (v.source[0] == 'n') v.source = "servidor";

    int pct = GameMemory::Player::PartyLifePercent(0);
    if (pct >= 0) v.lifePct = pct;

    return v;
}

// Diagnóstico das fontes (a cada 30s — o log principal pode ser limpo)
static void LogSourcesOnce()
{
    static DWORD lastLog = 0;
    const DWORD now = GetTickCount();
    if (lastLog != 0 && now - lastLog < 30000) return;
    lastLog = now;

    uintptr_t uo = GameMemory::Player::UserObject();
    int pmax = GameMemory::Player::PartyMaxLife(0);
    int plife = GameMemory::Player::PartyLife(0);
    int pct = GameMemory::Player::PartyLifePercent(0);
    Vitals v = ReadVitals();

    const int hpPct = (v.maxLife > 0) ? (int)((v.life * 100) / v.maxLife) : -1;
    const int mpPct = (v.maxMana > 0) ? (int)((v.mana * 100) / v.maxMana) : -1;

    unsigned cLife = 0, cMaxLife = 0, cMana = 0, cMaxMana = 0;
    const bool haveClient = GameMemory::Player::ReadClientVitals(cLife, cMaxLife, cMana, cMaxMana);

    Logger::Log("POTION", "fontes: objChar=%p | party: life=%d maxLife=%d pct=%d | "
                          "trafego: life=%u(%d,%dms) mana=%u(%d,%dms) max=%u/%u | "
                          "calc: life=%u/%u mana=%u/%u(%d,%dms) | "
                          "cliente: %s | DECISAO: life=%u/%u (%d%%) mana=%u/%u (%d%%)",
        (void*)uo,
        plife, pmax, pct,
        g_netLife, g_netLifeValid ? 1 : 0, (int)(now - g_netLifeTick),
        g_netMana, g_netManaValid ? 1 : 0, (int)(now - g_netManaTick),
        g_netMaxLife, g_netMaxMana,
        g_charLife, g_charMaxLife, g_charMana, g_charMaxMana,
        g_charStatsValid ? 1 : 0, (int)(now - g_charTick),
        haveClient ? "SIM" : "nao",
        v.life, v.maxLife, hpPct, v.mana, v.maxMana, mpPct);
}

// Origem do slot em uso (para a UI).
enum class SlotSource { None = 0, Manual = 1, Inventory = 2, ItemId = 3, Learned = 4 };

// Resolve o slot da potion e a sua origem.
//   Manual    → hpslot/mpslot configurado à mão
//   Inventory → detectado pelo ID do item no inventário (F3 10)
//   ItemId    → hpitem/mpitem configurado
//   Learned   → modelo aprendido (fallback do learn & replay)
static int ResolveSlot(bool isHp, SlotSource* source)
{
    int slot = isHp ? g_Config.hpSlot : g_Config.mpSlot;
    if (slot >= 0) { if (source) *source = SlotSource::Manual; return slot; }

    slot = isHp ? CurrentHpSlot() : CurrentMpSlot();
    if (slot >= 0) { if (source) *source = SlotSource::Inventory; return slot; }

    const int itemId = isHp ? g_Config.hpItemId : g_Config.mpItemId;
    if (itemId >= 0) {
        slot = FindSlotByItemId(itemId);
        if (slot >= 0) { if (source) *source = SlotSource::ItemId; return slot; }
    }

    EnterCriticalSection(&g_Lock);
    const bool learned = isHp ? g_hpModelValid : g_mpModelValid;
    LeaveCriticalSection(&g_Lock);
    if (learned) { if (source) *source = SlotSource::Learned; return -1; }

    if (source) *source = SlotSource::None;
    return -1;
}

// ============================================================================
// Estado para a UI (MuProxy2): "POTION_STATE|..." pelo canal de comandos.
//
// A UI mostra o slot em uso de HP/MP, a origem dele (inventário/aprendido) e o
// life/mana atual — para o usuário ver se o módulo está "vendo" o dano e não
// precisa mexer em nada.
// ============================================================================
static void SendStateToUi()
{
    SlotSource hpSrc = SlotSource::None, mpSrc = SlotSource::None;
    const int hpSlot = ResolveSlot(true, &hpSrc);
    const int mpSlot = ResolveSlot(false, &mpSrc);

    const Vitals v = ReadVitals();

    const char* hpTag = "inv";
    switch (hpSrc) {
        case SlotSource::Manual:    hpTag = "cfg"; break;
        case SlotSource::Inventory: hpTag = "inv"; break;
        case SlotSource::ItemId:    hpTag = "id";  break;
        case SlotSource::Learned:   hpTag = "apr"; break;
        default:                    hpTag = "-";   break;
    }
    const char* mpTag = "inv";
    switch (mpSrc) {
        case SlotSource::Manual:    mpTag = "cfg"; break;
        case SlotSource::Inventory: mpTag = "inv"; break;
        case SlotSource::ItemId:    mpTag = "id";  break;
        case SlotSource::Learned:   mpTag = "apr"; break;
        default:                    mpTag = "-";   break;
    }

    // Nome da potion em uso (vem do ID do item na tabela do inventário).
    const char* hpName = "";
    const char* mpName = "";
    {
        EnterCriticalSection(&g_Lock);
        if (hpSlot >= 0 && hpSlot < INVENTORY_SLOTS && g_slotItemId[hpSlot] >= 0)
            hpName = PotionName((unsigned)g_slotItemId[hpSlot]);
        if (mpSlot >= 0 && mpSlot < INVENTORY_SLOTS && g_slotItemId[mpSlot] >= 0)
            mpName = PotionName((unsigned)g_slotItemId[mpSlot]);
        LeaveCriticalSection(&g_Lock);
    }

    // Aviso: falta potion do tipo (o auto-potion não tem o que usar).
    const char* warn = "";
    if (hpSlot < 0 && mpSlot < 0) warn = "sem potion de HP/MP no inventario";
    else if (hpSlot < 0) warn = "sem potion de HP";
    else if (mpSlot < 0) warn = "sem potion de MP";

    CommandChannel::SendFormat(
    "POTION_STATE|active=%d|hp=%d|hpSrc=%s|hpName=%s|mp=%d|mpSrc=%s|mpName=%s|items=%d|"
    "life=%u|lifeMax=%u|mana=%u|manaMax=%u|warn=%s",
    IsRunning() ? 1 : 0, hpSlot, hpTag, hpName, mpSlot, mpTag, mpName, g_itemListCount,
    v.life, v.maxLife, v.mana, v.maxMana, warn);
}

// ============================================================================
// Monta o pacote de uso de item (6 bytes, plaintext).
//   isHp = true  → potion de HP
//   isHp = false → potion de MP
// Retorna false se não há modelo (e nem slot configurado).
// ============================================================================
static bool BuildItemUsePacket(bool isHp, uint8_t* outPacket)
{
    SlotSource source = SlotSource::None;
    const int slot = ResolveSlot(isHp, &source);

    // Slot resolvido (manual / inventário / ID de item): monta o pacote
    // (TargetSlot 0x00 e type 0 — é o que o jogo envia, captura 18:51:47:
    // C1 06 26 <slot> 00 00).
    if (slot >= 0) {
        outPacket[0] = (uint8_t)MuOpcodes::HeaderType::C1;
        outPacket[1] = MuOpcodes::Size::ItemUse;
        outPacket[2] = (uint8_t)MuOpcodes::ClientToServer::ItemUse;
        outPacket[3] = (uint8_t)slot;
        outPacket[4] = 0x00;
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
        g_lastBlock = BlockReason::Interval;
        return false;   // ainda dentro do intervalo mínimo
    }

    uint8_t packet[6];
    if (!BuildItemUsePacket(isHp, packet)) {
        g_lastBlock = BlockReason::NoModel;
        static int warnHp = 0, warnMp = 0;
        int& warn = isHp ? warnHp : warnMp;
        if (warn < 2) {
            warn++;
            Logger::Log("POTION", "sem potion de %s disponível: configure hpitem/mpitem (ID hex) "
                                  "ou beba a potion uma vez no jogo para o byp aprender o slot",
                        isHp ? "HP" : "MP");
        }
        return false;   // sem modelo aprendido e sem slot configurado
    }

    if (!PacketInjector::Inject(packet, 6)) {
        g_lastBlock = BlockReason::InjectFailed;
        Logger::Log("POTION", "falha ao injetar o uso de item (slot=%u)", packet[3]);
        return false;
    }

    g_lastUseTick = now;
    g_lastBlock = BlockReason::None;

    // Marca o uso para verificação: se o life/mana não subir, o slot não
    // serviu (potion acabou / servidor recusou) → troca de slot.
    {
        const Vitals v = ReadVitals();
        g_autoUseValid = true;
        g_autoUseIsHp = isHp;
        g_autoUseSlot = packet[3];
        g_autoUseLifeBefore = v.life;
        g_autoUseManaBefore = v.mana;
        g_autoUseTick = now;
    }

    Logger::Log("POTION", "%s disparada em %d%% (slot=%u) — uso injetado",
                isHp ? "potion de HP" : "potion de MP", percent, packet[3]);
    return true;
}

// ============================================================================
// Verificação do uso automático + troca de slot.
//
// Se a potion não produziu efeito (life/mana não subiu), o slot provavelmente
// está vazio (a potion acabou) ou o servidor recusou o uso. Nesses casos o
// byp passa para o próximo slot do mesmo tipo — sem depender do jogador.
// ============================================================================
static void VerifyAutoUse()
{
    if (!g_autoUseValid) return;

    const DWORD now = GetTickCount();
    if (now - g_autoUseTick < 900) return;   // espera o efeito chegar

    const Vitals v = ReadVitals();
    const bool worked = g_autoUseIsHp
        ? (v.haveLife && v.life >= g_autoUseLifeBefore + 3)
        : (v.haveMana && v.mana >= g_autoUseManaBefore + 3);

    const int oldSlot = g_autoUseSlot;
    const bool wasHp = g_autoUseIsHp;
    g_autoUseValid = false;

    if (worked) return;

    // O slot não serviu → próximo slot do mesmo tipo.
    int* list = wasHp ? g_hpSlots : g_mpSlots;
    int* count = wasHp ? &g_hpSlotCount : &g_mpSlotCount;
    int* idx = wasHp ? &g_hpSlotIdx : &g_mpSlotIdx;

    if (*count <= 1) {
        Logger::Log("POTION", "potion de %s no slot %d nao teve efeito e nao ha outro slot de %s "
                              "— reponha as potions ou configure h%slot/m%slot",
                    wasHp ? "HP" : "MP", oldSlot, wasHp ? "HP" : "MP");
        return;
    }

    *idx = (*idx + 1) % *count;
    Logger::Log("POTION", "potion de %s no slot %d nao teve efeito → usando slot %d",
                wasHp ? "HP" : "MP", oldSlot, list[*idx]);
}

// ============================================================================
// ResolvePendingModel: classifica o modelo observado como HP ou MP.
//
// Uma potion de verdade provoca um SALTO grande nos vitais (o servidor
// restaura 30~100% do máximo — CharacterUsePotion). Regeneração, dano ou
// outro item mudam poucos pontos: sem um limiar mínimo o "learn" classificava
// a potion de MP como sendo de HP (o life subiu 1 ponto por regeneração) e
// o byp passava a beber a potion errada.
//
// A classificação é tentada a CADA tick (o efeito pode chegar a qualquer
// momento); o timeout só encerra a espera sem classify.
// ============================================================================
static void ResolvePendingModel()
{
    if (!g_pendingValid) return;

    const DWORD now = GetTickCount();
    const Vitals v = ReadVitals();

    // 5% do máximo (mínimo de 3 pontos) = salto de potion.
    unsigned minLifeDelta = (v.maxLife > 0) ? (v.maxLife / 20) : 3;
    unsigned minManaDelta = (v.maxMana > 0) ? (v.maxMana / 20) : 3;
    if (minLifeDelta < 3) minLifeDelta = 3;
    if (minManaDelta < 3) minManaDelta = 3;

    // O PICO (máximo) durante a janela, e não o valor final: se o jogador
    // toma dano / gasta mana no mesmo instante em que bebe a potion, o valor
    // final esconde o efeito. O pico prova que a potion foi aplicada.
    unsigned beforeLife = 0, beforeMana = 0, peakLife = 0, peakMana = 0;
    uint8_t slot = 0;
    {
        EnterCriticalSection(&g_Lock);
        beforeLife = g_pendingLife;
        beforeMana = g_pendingMana;
        if (v.life > g_pendingPeakLife) g_pendingPeakLife = v.life;
        if (v.mana > g_pendingPeakMana) g_pendingPeakMana = v.mana;
        peakLife = g_pendingPeakLife;
        peakMana = g_pendingPeakMana;
        slot = g_pendingModel[3];
        LeaveCriticalSection(&g_Lock);
    }

    const bool hpUp = v.haveLife && peakLife >= beforeLife + minLifeDelta;
    const bool mpUp = v.haveMana && peakMana >= beforeMana + minManaDelta;

    if (!hpUp && !mpUp) {
        // Sem salto relevante ainda — só desiste depois do timeout.
        if (now - g_pendingTick < (DWORD)PENDING_TIMEOUT_MS) return;
        Logger::Log("POTION", "uso observado sem efeito relevante (slot=%u): life %u->%u (pico %u), "
                              "mana %u->%u (pico %u) — precisa de +%u/+%u. "
                              "Beba a potion com o recurso ABAIXO do máximo para o byp aprender.",
                    slot, beforeLife, v.life, peakLife, beforeMana, v.mana, peakMana,
                    minLifeDelta, minManaDelta);
        g_pendingValid = false;
        return;
    }

    EnterCriticalSection(&g_Lock);
    if (hpUp && !mpUp) {
        memcpy(g_hpModel, g_pendingModel, 6);
        g_hpModelValid = true;
        Logger::Log("POTION", "modelo de HP aprendido (slot=%u)", g_hpModel[3]);
    } else if (mpUp && !hpUp) {
        memcpy(g_mpModel, g_pendingModel, 6);
        g_mpModelValid = true;
        Logger::Log("POTION", "modelo de MP aprendido (slot=%u)", g_mpModel[3]);
    } else {
        Logger::Log("POTION", "uso observado com life E mana subindo (slot=%u) — ambíguo, "
                              "mantido o modelo anterior", slot);
    }
    g_pendingValid = false;
    LeaveCriticalSection(&g_Lock);
}

// ============================================================================
// Verifica os limiares e dispara a potion correspondente.
// ============================================================================
static void CheckThresholds()
{
    LogSourcesOnce();

    // Procura (uma vez a cada 5s até achar) os globals da HUD no main.exe:
    // é a única fonte do MÁXIMO de life/mana nesse servidor.
    if (!GameMemory::Player::HasClientVitals() &&
        g_hints.lifeCount > 0 && g_hints.manaCount > 0) {
        static DWORD lastScan = 0;
        const DWORD now = GetTickCount();
        if (lastScan == 0 || now - lastScan >= 5000) {
            lastScan = now;
            GameMemory::Player::ScanClientVitals(g_hints);
        }
    }

    Vitals v = ReadVitals();
    if (!v.haveLife && !v.haveMana) {
        g_lastBlock = BlockReason::NoVitals;
        static int warn = 0;
        if (warn < 3) {
            warn++;
            Logger::Log("POTION", "sem vitais do jogador (life/mana indisponíveis) — "
                                  "aguardando os pacotes do servidor");
        }
        return;   // sem dados do jogador
    }

    // %: calculado a partir da fonte mais recente do servidor.
    // (A party list do jogo não é confiável neste cliente — o slot 0 vem
    //  zerado — então ela só entra como último recurso.)
    int hpPercent = 100;
    if (v.haveLife && v.maxLife > 0) hpPercent = (int)((v.life * 100) / v.maxLife);
    else if (v.haveLife && v.lifePct >= 0) hpPercent = v.lifePct;
    else if (v.haveLife) {
        static int warn = 0;
        if (warn < 3) {
            warn++;
            Logger::Log("POTION", "life=%u conhecido mas maxLife DESCONHECIDO — "
                                  "%% não pode ser calculado", v.life);
        }
    }

    int mpPercent = 100;
    if (v.haveMana && v.maxMana > 0) mpPercent = (int)((v.mana * 100) / v.maxMana);
    else if (v.haveMana) {
        static int warn = 0;
        if (warn < 3) {
            warn++;
            Logger::Log("POTION", "mana=%u conhecido mas maxMana DESCONHECIDO", v.mana);
        }
    }

    g_lastBlock = BlockReason::None;
    bool tried = false;

    if (v.haveLife && hpPercent < g_Config.hpThreshold) {
        tried = true;
        if (UsePotion(true, hpPercent)) return;   // uma potion por vez
    }
    if (v.haveMana && mpPercent < g_Config.mpThreshold) {
        tried = true;
        if (UsePotion(false, mpPercent)) return;
    }

    // O limiar foi atingido mas a potion NÃO saiu — diz o porquê (a cada 5s).
    if (tried && g_lastBlock != BlockReason::None) {
        static DWORD lastLog = 0;
        const DWORD now = GetTickCount();
        if (lastLog == 0 || now - lastLog >= 5000) {
            lastLog = now;
            const char* why = "?";
            switch (g_lastBlock) {
                case BlockReason::NoModel:      why = "sem modelo de potion aprendido"; break;
                case BlockReason::Interval:     why = "dentro do intervalo mínimo entre usos"; break;
                case BlockReason::InjectFailed: why = "falha na injecao do pacote"; break;
                case BlockReason::NoVitals:     why = "sem vitais do jogador"; break;
                default: break;
            }
            Logger::Log("POTION", "limiar atingido (hp=%d%% mp=%d%%) mas a potion nao saiu: %s",
                        hpPercent, mpPercent, why);
        }
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
            VerifyAutoUse();
            CheckThresholds();
        }

        // Estado para a UI (a cada ~1s) — mostra slot em uso, life/mana e
        // o aviso quando falta potion.
        {
            static DWORD lastState = 0;
            const DWORD now = GetTickCount();
            if (lastState == 0 || now - lastState >= 1000) {
                lastState = now;
                SendStateToUi();
            }
        }

        Sleep(TICK_MS);
    }

    Logger::Log("POTION", "Watcher encerrado");
    return 0;
}

// ============================================================================
// Handlers de mensagens tipadas (MessageRegistry)
// ============================================================================

// Aplica os campos de configuração (mesmo formato do POTION_CONFIG).
static void ApplyConfigPayload(const char* payload)
{
    if (!payload || !*payload) return;

    int ival; uint16_t hval;
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
    // ID do item (hex) — resolve o slot pela tabela do inventário (F3 10).
    if (CommandParams::ParseHexParam(payload, "hpitem", &hval))
        g_Config.hpItemId = (int)hval;
    if (CommandParams::ParseHexParam(payload, "mpitem", &hval))
        g_Config.mpItemId = (int)hval;

    Logger::Log("POTION", "config: hp<%d%% mp<%d%% intervalo=%dms hpslot=%d mpslot=%d hpitem=0x%04X mpitem=0x%04X",
                g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs,
                g_Config.hpSlot, g_Config.mpSlot,
                g_Config.hpItemId, g_Config.mpItemId);
}

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
    ApplyConfigPayload(payload);
}
REGISTER_MESSAGE(POTION_CONFIG, HandlePotionConfig);

// POTION_APPLY — UMA mensagem que aplica a configuração E arma o módulo
// (idempotente). A UI manda isso a cada mudança (checkbox/limiar/intervalo)
// e quando o byp.dll reconecta (o módulo volta do zero com o jogo).
static void HandlePotionApply(const char* payload)
{
    // ⚠️ Nenhuma mensagem é descartada aqui: a UI já tem debounce (400 ms) e
    // o "debounce" antigo (1 s) chegou a engolir uma configuração legítima
    // (usuário mexendo no limiar 2x em 1s) — a UI ficava mostrando um valor
    // e o módulo usava outro.
    ApplyConfigPayload(payload);

    const LONG wasActive = InterlockedExchange(&g_Active, 1);
    if (wasActive == 0) {
        Logger::Log("POTION", "auto-potion LIGADO (hp<%d%% mp<%d%% intervalo=%dms)",
                    g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs);
        LogStatus();
    }
}
REGISTER_MESSAGE(POTION_APPLY, HandlePotionApply);

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

// ============================================================================
// Inventário (F3 10) — tabela slot → ID do item
//
// Layout (servidor eMU: CItemManager::GCItemListSend / PMSG_ITEM_LIST_SEND):
//   [C2/C3 <size>] F3 10 <count> [slot(1) + ItemInfo(12)] * count
//
// O ID do item é montado a partir do ItemInfo (CItemManager::ItemByteConvert):
//   byte0            → índice bits 0..7
//   byte3 bit 1      → índice bit 8
//   byte5 bits 4..7  → índice bits 9..12 (o "grupo" = índice >> 9)
//
// ⚠️ O servidor usa GET_ITEM(grupo,idx) = grupo*512 + idx (MAX_ITEM_TYPE=512),
// então os bits do grupo ficam em 9..12 — não em 8..11.
// ⚠️ E o grupo é o NIBBLE ALTO do byte5: o servidor grava
// `lpMsg[5] |= (m_Index & GET_ITEM(15,0)) >> 5` (CItemManager::ItemByteConvert),
// ou seja, os bits 9..12 do índice caem nos bits 4..7 do byte5. Ler os bits
// 3..6 (o erro antigo) devolvia o grupo pela metade: o "Scroll of Twister"
// virava "Siege Potion" e as potions de verdade passavam despercebidas.
// ============================================================================
static unsigned DecodeItemId(const uint8_t* info)
{
    unsigned id = info[0];
    id |= (unsigned)((info[3] & 0x02) << 7);          // índice bit 8
    id |= (unsigned)(((info[5] >> 4) & 0x0F) << 9);   // índice bits 9..12 (grupo)
    return id;
}

// ============================================================================
// Observação do servidor: um único ponto de entrada para os pacotes S→C.
// ============================================================================

// ============================================================================
// Decodifica (de forma silenciosa) o corpo cifrado de um pacote C3/C4.
// O servidor usa SimpleModulus nos pacotes C3/C4 (SocketManager::DataSend).
//
// O envelope no wire:
//   C3 → [C3][size:1][SM cifrado]              (header de 2 bytes)
//   C4 → [C4][sizeHi][sizeLo][SM cifrado]       (header de 3 bytes)
//
// O corpo decifrado começa com o byte do serial; depois vem o conteúdo do
// pacote interno (no servidor: o header PSBMSG_HEAD, ex.: [sizeHi][sizeLo]
// [0xF3][0x10] e o payload).
// ============================================================================
static bool TryDecodeSmBody(const uint8_t* data, int len, int hdrSize,
                             uint8_t* out, int outCap, int* outLen)
{
    if (!data || len < hdrSize + SM_ENCRYPTED_SIZE) return false;
    if (data[0] != (uint8_t)MuOpcodes::HeaderType::C3 &&
        data[0] != (uint8_t)MuOpcodes::HeaderType::C4) return false;

    const int smLen = len - hdrSize;
    if (smLen < SM_ENCRYPTED_SIZE) return false;

    // Cada bloco de 11 bytes vira 8 bytes: o Decrypt escreve (smLen/11)*8.
    // Não pode estourar o buffer de destino.
    if ((smLen / SM_ENCRYPTED_SIZE) * SM_BLOCK_SIZE > outCap) return false;

    SimpleModulusCipher::SetVerbose(false);
    int decLen = SimpleModulusCipher::Decrypt(out, data + hdrSize, smLen);
    if (decLen <= 0) {
        // O SERVIDOR cifra os C3/C4 com a chave de RECEPÇÃO (Obj2 do cliente) —
        // com a chave de envio o checksum do SimpleModulus falha.
        decLen = SimpleModulusCipher::DecryptReceive(out, data + hdrSize, smLen);
    }
    SimpleModulusCipher::SetVerbose(true);

    if (decLen <= 0) return false;
    *outLen = (decLen > outCap) ? outCap : decLen;
    return true;
}
// ============================================================================
// F3 E0 — character info (vida/mana do JOGADOR).
// payload: Level(2) LevelUpPoint(2) Exp(4) NextExp(4) Str(2) Agi(2) Vit(2)
//          Ene(2) Leadership(2) Life(2) MaxLife(2) Mana(2) MaxMana(2) ...
// (WORDs little-endian)
// ============================================================================
static void ParseCharacterInfo(const uint8_t* p, int len)
{
    if (len < 30) return;

    auto rd16 = [](const uint8_t* b) -> unsigned {
        return (unsigned)b[0] | ((unsigned)b[1] << 8);   // little-endian
    };

    const unsigned level  = rd16(p + 0);
    const unsigned life   = rd16(p + 22);
    const unsigned maxLife= rd16(p + 24);
    const unsigned mana   = rd16(p + 26);
    const unsigned maxMana= rd16(p + 28);

    if (maxLife > 0) {
        g_charLife     = life;
        g_charMaxLife  = maxLife;
        g_charMana     = mana;
        g_charMaxMana  = maxMana;
        g_charStatsValid = true;
        g_charTick = GetTickCount();
    }

    Logger::Log("POTION", "character info (F3 E0): level=%u life=%u/%u mana=%u/%u",
                level, life, maxLife, mana, maxMana);
}

// ============================================================================
// F3 E1 — character calc: vida/mana ATUAL + máximo.
// payload: CurHP(4) MaxHP(4) CurMP(4) MaxMP(4) CurBP(4) MaxBP(4) CurSD(4) ...
// (DWORDs little-endian)
//
// O servidor manda este pacote junto com o update de life (C1 26) — os dois
// descrevem o MESMO personagem (o jogador local). É daqui que sai o
// MaxLife (sem ele não dá para calcular o %).
// ============================================================================
static void ParseCharacterCalc(const uint8_t* p, int len)
{
    if (len < 32) return;

    auto rd32 = [](const uint8_t* b) -> unsigned {
        return (unsigned)b[0] | ((unsigned)b[1] << 8) |
               ((unsigned)b[2] << 16) | ((unsigned)b[3] << 24);   // little-endian
    };

    const unsigned curHP  = rd32(p + 0);
    const unsigned maxHP  = rd32(p + 4);
    const unsigned curMP  = rd32(p + 8);
    const unsigned maxMP  = rd32(p + 12);
    const unsigned curBP  = rd32(p + 16);
    const unsigned curSD  = rd32(p + 24);

    if (maxHP == 0) return;

    // Sanidade: os valores têm de ser coerentes entre si (atual <= máximo).
    // Não se compara com o tráfego: um update de life antigo (ou um pacote
    // type 0xFE com o máximo) rejeitaria um calc válido.
    if (curHP > maxHP + 1000 || curMP > maxMP + 1000) return;

    g_charLife     = curHP;
    g_charMaxLife  = maxHP;
    g_charMana     = curMP;
    g_charMaxMana  = maxMP;
    g_charStatsValid = true;
    g_charTick = GetTickCount();

    static int logCount = 0;
    logCount++;
    if (logCount <= 5 || (logCount % 20) == 0) {
        Logger::Log("POTION", "character calc (F3 E1): life=%u/%u mana=%u/%u bp=%u sd=%u",
                    curHP, maxHP, curMP, maxMP, curBP, curSD);
    }
}

// F3 10 — lista do inventário: payload = <count> [slot(1) + ItemInfo(12)]*
static void ParseItemList(const uint8_t* p, int len)
{
    if (len < 1) return;

    const int count = p[0];
    const int ITEM_SIZE = 1 + 12;              // slot + ItemInfo
    if (count <= 0 || count > INVENTORY_SLOTS) return;
    if (1 + count * ITEM_SIZE > len) {
        Logger::Log("POTION", "lista de itens truncada (count=%d len=%d) — ignorada", count, len);
        return;
    }

    EnterCriticalSection(&g_Lock);
    for (int i = 0; i < INVENTORY_SLOTS; i++) g_slotItemId[i] = -1;
    g_itemListCount = count;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "lista de itens (F3 10): %d item(ns)", count);
    for (int i = 0; i < count; i++) {
        const uint8_t* item = p + 1 + i * ITEM_SIZE;
        const int slot = item[0];
        const unsigned id = DecodeItemId(item + 1);
        if (slot >= 0 && slot < INVENTORY_SLOTS) {
            EnterCriticalSection(&g_Lock);
            g_slotItemId[slot] = (int)id;
            LeaveCriticalSection(&g_Lock);
            Logger::Log("POTION", "  slot=%d item=0x%04X (grupo %u, idx %u)%s%s",
                        slot, id, id / 512u, id % 512u,
                        PotionName(id)[0] ? " " : "", PotionName(id));
        }
    }

    // Descobre automaticamente os slots de potion (HP/MP) pelo ID do item.
    RebuildPotionSlots();
}

// ============================================================================
// F3 14 — item modificado (PMSG_ITEM_MODIFY_SEND).
//   payload: slot(1) + ItemInfo(12)
// O servidor manda isso a cada mudança no inventário (pegar/gastar/mover item)
// — é aqui que o módulo descobre uma potion nova sem esperar o próximo login.
// ============================================================================
static void ParseItemModify(const uint8_t* p, int len)
{
    if (len < 13) return;

    const int slot = p[0];
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;

    const unsigned id = DecodeItemId(p + 1);

    EnterCriticalSection(&g_Lock);
    g_slotItemId[slot] = (int)id;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "item modificado (F3 14): slot=%d item=0x%04X (grupo %u, idx %u)%s%s",
                slot, id, id / 512u, id % 512u,
                PotionName(id)[0] ? " " : "", PotionName(id));

    RebuildPotionSlots();
}

// ============================================================================
// C1 28 — item removido do inventário (PMSG_ITEM_DELETE_SEND).
//   payload: slot(1) + flag(1)
// É o que acontece quando a potion é gasta: o slot tem de sair da tabela,
// senão o módulo fica usando um slot vazio.
// ============================================================================
static void ParseItemDelete(const uint8_t* p, int len)
{
    if (len < 2) return;

    const int slot = p[0];
    if (slot < 0 || slot >= INVENTORY_SLOTS) return;

    EnterCriticalSection(&g_Lock);
    const int old = g_slotItemId[slot];
    g_slotItemId[slot] = -1;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "item removido (C1 28): slot=%d (era 0x%04X)", slot, old);

    RebuildPotionSlots();
}

void OnServerPacket(const uint8_t* data, int len)
{
    if (!data || len < 4) return;

    const uint8_t* buf = data;
    int bufLen = len;

    // Pacote C3/C4: o corpo vem cifrado (SimpleModulus) → decodifica.
    //   C3 → header de 2 bytes   C4 → header de 3 bytes
    //
    // O C4 do login tem ~784 bytes (lista de itens + equipamento) → decifra
    // (781/11)*8 = 568 bytes. O buffer precisa comportar isso.
    uint8_t decoded[2048];
    int decLen = 0;
    if (data[0] == (uint8_t)MuOpcodes::HeaderType::C3 ||
        data[0] == (uint8_t)MuOpcodes::HeaderType::C4) {
        const int hdrSize = (data[0] == (uint8_t)MuOpcodes::HeaderType::C3) ? 2 : 3;
        if (TryDecodeSmBody(data, len, hdrSize, decoded, (int)sizeof(decoded), &decLen)) {
            // O corpo decifrado começa com o byte do serial; o conteúdo do
            // pacote interno vem depois (no servidor: o header PSBMSG_HEAD,
            // ex.: [sizeHi][sizeLo][0xF3][0x10] + payload).
            buf = decoded + 1;
            bufLen = decLen - 1;
            static int decLogged = 0;
            if (decLogged < 40) {
                decLogged++;
                PacketDiag::LogHex((data[0] == (uint8_t)MuOpcodes::HeaderType::C3)
                                       ? "C3-SM decodificado" : "C4-SM decodificado",
                                   decoded, decLen);
            }
        }
        else {
            static int failCount = 0;
            if (failCount < 5) {
                failCount++;
                Logger::Log("POTION", "%s do servidor (len=%d) nao decodificou (SimpleModulus) "
                                      "— o corpo fica cifrado",
                            data[0] == (uint8_t)MuOpcodes::HeaderType::C3 ? "C3" : "C4", len);
            }
            bufLen = 0;   // não dá para ler o payload
        }
    }
    else if (data[0] == (uint8_t)MuOpcodes::HeaderType::C1) {
        // corpo em claro: o payload começa depois do header (2 bytes)
        buf = data + 2;
        bufLen = len - 2;
    }

    // Procura os sub-códigos F3 (E0 = character info, E1 = calc, 10 = inventário).
    for (int i = 0; i + 3 < bufLen && i < 16; i++) {
        if (buf[i] != 0xF3) continue;
        const uint8_t sub = buf[i + 1];
        if (sub == (uint8_t)MuOpcodes::F3Sub::CharacterInfo)
            ParseCharacterInfo(buf + i + 2, bufLen - i - 2);
        else if (sub == (uint8_t)MuOpcodes::F3Sub::CharacterCalc)
            ParseCharacterCalc(buf + i + 2, bufLen - i - 2);
        else if (sub == (uint8_t)MuOpcodes::F3Sub::ItemList)
            ParseItemList(buf + i + 2, bufLen - i - 2);
        else if (sub == (uint8_t)MuOpcodes::F3Sub::ItemModify)
            ParseItemModify(buf + i + 2, bufLen - i - 2);
    }

    // Life/mana (C1 26 / C1 27) — payload cru (o servidor NÃO usa Camada-2
    // no sentido S→C).
    if (data[0] == (uint8_t)MuOpcodes::HeaderType::C1) {
        if (data[2] == (uint8_t)MuOpcodes::ServerToClient::LifeUpdate)
            OnLifeUpdatePacket(data, len);
        else if (data[2] == (uint8_t)MuOpcodes::ServerToClient::ManaUpdate)
            OnManaUpdatePacket(data, len);
        else if (data[2] == 0x28)   // PMSG_ITEM_DELETE_SEND (item removido)
            ParseItemDelete(buf + 1, bufLen - 1);
    }
}

void OnItemListPacket(const uint8_t* data, int len)
{
    if (!data || len < 6) return;

    // Localiza o cabeçalho F3 10 (tolera C1/C2/C3/C4 no início).
    int hdr = -1;
    for (int i = 0; i + 5 < len && i < 8; i++) {
        if (data[i + 3] == 0xF3 && data[i + 4] == 0x10) { hdr = i; break; }
    }
    if (hdr < 0) return;

    const int count = data[hdr + 5];
    const int ITEM_SIZE = 1 + 12;              // slot + ItemInfo
    if (count <= 0 || count > INVENTORY_SLOTS) return;
    if (hdr + 6 + count * ITEM_SIZE > len) {
        Logger::Log("POTION", "lista de itens truncada (count=%d len=%d) — ignorada", count, len);
        return;
    }

    EnterCriticalSection(&g_Lock);
    for (int i = 0; i < INVENTORY_SLOTS; i++) g_slotItemId[i] = -1;
    g_itemListCount = count;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "lista de itens (F3 10): %d item(ns)", count);
    for (int i = 0; i < count; i++) {
        const uint8_t* item = data + hdr + 6 + i * ITEM_SIZE;
        const int slot = item[0];
        const unsigned id = DecodeItemId(item + 1);
        if (slot >= 0 && slot < INVENTORY_SLOTS) {
            EnterCriticalSection(&g_Lock);
            g_slotItemId[slot] = (int)id;
            LeaveCriticalSection(&g_Lock);
            Logger::Log("POTION", "  slot=%d item=0x%04X (grupo %u, idx %u)%s%s",
                        slot, id, id / 512u, id % 512u,
                        PotionName(id)[0] ? " " : "", PotionName(id));
        }
    }
}

int FindSlotByItemId(int itemId)
{
    if (itemId < 0) return -1;

    EnterCriticalSection(&g_Lock);
    int found = -1;
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (g_slotItemId[i] == itemId) { found = i; break; }
    }
    LeaveCriticalSection(&g_Lock);
    return found;
}

void OnItemUsePacket(const uint8_t* data, int len){
    if (!data || len != 6) return;
    if (data[0] != (uint8_t)MuOpcodes::HeaderType::C1) return;
    if (data[2] != (uint8_t)MuOpcodes::ClientToServer::ItemUse) return;

    // O sniffer só desfez a Layer-1: o payload ainda está na forma Camada-2.
    uint8_t plain[6];
    memcpy(plain, data, 6);
    MuCipher::UnXorData((char*)plain, 3, 6);

    // Snapshot dos vitais ANTES do uso (para classificar HP vs MP depois).
    // Usa a fonte do servidor (rede) — a memória do jogo não tem o objeto
    // do jogador local válido.
    const Vitals before = ReadVitals();

    EnterCriticalSection(&g_Lock);
    memcpy(g_pendingModel, plain, 6);
    g_pendingLife = before.life;
    g_pendingMana = before.mana;
    g_pendingPeakLife = before.life;
    g_pendingPeakMana = before.mana;
    g_pendingTick = GetTickCount();
    g_pendingValid = true;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "uso de item observado (cliente): slot=%u target=0x%02X type=0x%02X (life=%u mana=%u)",
                plain[3], plain[4], plain[5], before.life, before.mana);
}

void LogStatus()
{
    Vitals v = ReadVitals();
    unsigned life = v.life, maxLife = v.maxLife;
    unsigned mana = v.mana, maxMana = v.maxMana;
    const int hpPct = maxLife ? (int)((life * 100) / maxLife) : -1;
    const int mpPct = maxMana ? (int)((mana * 100) / maxMana) : -1;

    EnterCriticalSection(&g_Lock);
    bool hpValid = g_hpModelValid;
    bool mpValid = g_mpModelValid;
    unsigned hpSlot = hpValid ? g_hpModel[3] : 0;
    unsigned mpSlot = mpValid ? g_mpModel[3] : 0;
    LeaveCriticalSection(&g_Lock);

    Logger::Log("POTION", "estado: %s | hp<%d%% mp<%d%% intervalo=%dms | modelo HP=%s (slot %u) | modelo MP=%s (slot %u) | life=%u/%u (%d%%) mana=%u/%u (%d%%)",
                IsRunning() ? "LIGADO" : "DESLIGADO",
                g_Config.hpThreshold, g_Config.mpThreshold, g_Config.intervalMs,
                hpValid ? "ok" : "-", hpSlot, mpValid ? "ok" : "-", mpSlot,
                life, maxLife, hpPct, mana, maxMana, mpPct);

    Logger::Log("POTION", "  slots automaticos: HP=%d (de %d) MP=%d (de %d) | inventario=%d item(ns)",
                CurrentHpSlot(), g_hpSlotCount, CurrentMpSlot(), g_mpSlotCount, g_itemListCount);

    if (g_Config.hpSlot >= 0) Logger::Log("POTION", "  slot manual de HP: %d", g_Config.hpSlot);
    if (g_Config.mpSlot >= 0) Logger::Log("POTION", "  slot manual de MP: %d", g_Config.mpSlot);
    if (g_Config.hpItemId >= 0) {
        int s = FindSlotByItemId(g_Config.hpItemId);
        Logger::Log("POTION", "  item de HP 0x%04X -> slot %d", g_Config.hpItemId, s);
    }
    if (g_Config.mpItemId >= 0) {
        int s = FindSlotByItemId(g_Config.mpItemId);
        Logger::Log("POTION", "  item de MP 0x%04X -> slot %d", g_Config.mpItemId, s);
    }
    Logger::Log("POTION", "  inventário conhecido: %d item(ns)", g_itemListCount);
    Logger::Log("POTION", "  vitals: life=%u/%u mana=%u/%u (fonte: %s)", life, maxLife, mana, maxMana, v.source);
}

// ============================================================================
// Updates de life/mana do servidor (S→C)
//   C1 26 <type> <lifeHi> <lifeLo> <flag> <shieldHi> <shieldLo>
//   C1 27 <type> <manaHi> <manaLo> <bpHi> <bpLo>
// O life/mana são big-endian (SET_NUMBERHB/LB no servidor).
//
// ⚠️ O byte <type> é o que decide o SIGNIFICADO do valor:
//   0xFF → vida/mana ATUAL
//   0xFE → vida/mana MÁXIMA   (o servidor manda o max do personagem)
//   0xFD → vida atual (o servidor trunca em BYTE)
// Tratar o 0xFE como vida atual congela o auto-potion com a vida no máximo.
//
// ⚠️ O servidor NÃO aplica a Camada-2 (XorFilter) no sentido S→C: o
// SocketManager::DataSend só faz SimpleModulus (nos C3/C4) + Layer-1.
// A Camada-2 é do sentido C→S (o servidor a desfaz em ExtractPacket).
// Por isso aqui o payload é lido CRU.
// ============================================================================
static const uint8_t VITALS_TYPE_CURRENT = 0xFF;
static const uint8_t VITALS_TYPE_MAXIMUM = 0xFE;

void OnLifeUpdatePacket(const uint8_t* data, int len)
{
    if (!data || len < 6) return;
    if (data[0] != (uint8_t)MuOpcodes::HeaderType::C1) return;
    if (data[2] != (uint8_t)MuOpcodes::ServerToClient::LifeUpdate) return;

    const uint8_t type = data[3];
    const unsigned life = ((unsigned)data[4] << 8) | data[5];

    // 0xFE = MÁXIMO do personagem (não a vida atual).
    if (type == VITALS_TYPE_MAXIMUM) {
        if (life > g_netMaxLife) g_netMaxLife = life;
        static int maxLog = 0;
        if (maxLog < 3) {
            maxLog++;
            Logger::Log("POTION", "life update MAXIMO (type=0x%02X maxLife=%u)", type, life);
        }
        return;
    }

    g_netLife = life;
    g_netLifeValid = true;
    g_netLifeTick = GetTickCount();
    PushHint(g_hints.life, g_hints.lifeCount, 8, life);

    // Escudo atual (o mesmo campo que o cliente usa na HUD).
    if (len >= 9) {
        const unsigned shield = ((unsigned)data[7] << 8) | data[8];
        PushHint(g_hints.shield, g_hints.shieldCount, 4, shield);
    }

    static int logCount = 0;
    logCount++;
    if (logCount <= 5 || (logCount % 20) == 0) {
        Logger::Log("POTION", "life update: type=0x%02X life=%u (bytes %02X %02X)",
                    type, life, data[4], data[5]);
    }
}

void OnManaUpdatePacket(const uint8_t* data, int len)
{
    if (!data || len < 6) return;
    if (data[0] != (uint8_t)MuOpcodes::HeaderType::C1) return;
    if (data[2] != (uint8_t)MuOpcodes::ServerToClient::ManaUpdate) return;

    const uint8_t type = data[3];
    const unsigned mana = ((unsigned)data[4] << 8) | data[5];

    // 0xFE = MÁXIMO de mana (não a mana atual).
    if (type == VITALS_TYPE_MAXIMUM) {
        if (mana > g_netMaxMana) g_netMaxMana = mana;
        static int maxLog = 0;
        if (maxLog < 3) {
            maxLog++;
            Logger::Log("POTION", "mana update MAXIMO (type=0x%02X maxMana=%u)", type, mana);
        }
        return;
    }

    g_netMana = mana;
    g_netManaValid = true;
    g_netManaTick = GetTickCount();
    PushHint(g_hints.mana, g_hints.manaCount, 8, mana);

    // BP atual (constante nesse servidor — 160 — ótimo para a assinatura).
    if (len >= 8) {
        const unsigned bp = ((unsigned)data[6] << 8) | data[7];
        PushHint(g_hints.bp, g_hints.bpCount, 4, bp);
    }

    static int logCount = 0;
    logCount++;
    if (logCount <= 5 || (logCount % 20) == 0) {
        Logger::Log("POTION", "mana update: type=0x%02X mana=%u (bytes %02X %02X)",
                    type, mana, data[4], data[5]);
    }
}

// ============================================================================
// Character info do servidor (S→C): C1 F3 03 (ou C3 + SimpleModulus).
//
// Processado por ParseCharacterInfo (chamado de OnServerPacket).
// ============================================================================

} // namespace AutoPotion
