#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// AutoPotion — Bebida automática de potion (HP/MP)
//
// O byp.dll lê o life/mana do jogador local em memória
// (GameMemory::Player) e, quando o percentual cai abaixo do limiar, injeta o
// pacote de uso de item do jogo:
//
//   C1 06 26 <SourceSlot> <TargetSlot> <type>      (MuOpcodes::ClientToServer::ItemUse)
//
// O servidor (CItemManager::CGItemUseRecv → CObjectManager::CharacterUsePotion)
// aplica o efeito conforme o item que está no slot (potions GET_ITEM(14,0..6),
// SD 14,35..40, elite 14,70/71/133) e responde com o update de life/mana.
//
// 🕐 O slot do inventário: o cliente sabe o slot exato, o byp não (o
// inventário do cliente não está mapeado). Por isso o módulo é "learn & replay":
// quando o JOGADOR bebe uma potion, o sniffer entrega o C1 06 26 que ele
// mandou; o módulo decodifica (Camada-2 inversa), guarda o pacote como modelo
// e compara o life/mana antes e depois para descobrir se aquele modelo é de
// HP ou de MP. Também dá para configurar o slot à mão.
//
// ⚠️ Anti-hack: o servidor rejeita um segundo uso dentro de
// `CheckAutoPotionHackTolerance` ms (e NÃO consome a potion). Por isso o
// intervalo mínimo entre usos (default 1000 ms).
//
// Comandos (MessageRegistry, formato "TAG|payload"):
//   POTION_START                      → liga o auto-potion
//   POTION_STOP                       → desliga
//   POTION_CONFIG|hp=50|mp=30|interval=1000|hpslot=25|mpslot=26
//   POTION_STATUS                     → loga o estado atual
// ============================================================================

namespace AutoPotion {

// ---- Configuração ----
// (nome "Settings" e não "Config" para não colidir com o namespace Config
//  de include/Config.h)
struct Settings {
    int hpThreshold = 50;    // % de life que dispara a potion de HP
    int mpThreshold = 30;    // % de mana que dispara a potion de MP
    int intervalMs  = 1000;  // intervalo mínimo entre usos (anti-hack do servidor)
    int hpSlot      = -1;    // slot do inventário da potion de HP (-1 = usa o modelo aprendido)
    int mpSlot      = -1;    // slot do inventário da potion de MP (-1 = usa o modelo aprendido)
};

// ---- Ciclo de vida ----
bool Initialize();
void Shutdown();

// Liga/desliga o auto-potion (o watcher thread fica vivo nos dois estados).
void Start();
void Stop();
bool IsRunning();

// ---- Configuração em runtime ----
const Settings& GetConfig();
void SetConfig(const Settings& cfg);

// ---- Observação do tráfego ----
// Chamado pelo PacketSniffer quando o JOGADOR usa um item (C1 06 26, ainda
// na forma Camada-2 — a decodificação é feita aqui dentro).
void OnItemUsePacket(const uint8_t* data, int len);

// ---- Diagnóstico ----
void LogStatus();

} // namespace AutoPotion
