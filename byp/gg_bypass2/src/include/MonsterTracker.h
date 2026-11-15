#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// MonsterTracker — Escaneia o array de objetos e envia dados via TCP
//
// Lê o array de 400 slots em memória, filtra monstros/NPCs/jogadores
// e envia snapshots para o MuProxy2, incluindo distância do local player.
//
// Comandos recebidos via PacketHandler:
//   CMD MONSTER LIST [filter=<typeMask>]
//     → envia lista completa (opcionalmente filtrada por tipo)
//   CMD MONSTER START [interval=<ms>] [filter=<typeMask>] [range=<float>]
//     → inicia envio periódico com parâmetros
//   CMD MONSTER STOP
//     → para envio periódico
//   CMD MONSTER SINGLE [filter=<typeMask>]
//     → envia snapshot único
//   CMD MONSTER CONFIG interval=<ms> range=<float> filter=<typeMask>
//     → altera configurações padrão
//
// Formato OBJ expandido:
//   "OBJ <index> <compositeIdx> <type> <level> <x> <y> <z> <dist> <name>"
//   dist = distância euclidiana do local player (0.0 se local não disponível)
// ============================================================================

namespace MonsterTracker {

// Configurações dinâmicas (alteráveis via TCP)
struct TrackerConfig {
    int   scanIntervalMs;   // Intervalo entre scans (ms)
    float maxRange;          // Distância máxima para reportar (0 = sem limite)
    int   filterMask;        // Mask de tipos: bit 0=empty, 1=player, 2=monster, 4=npc
};

// Inicializa o tracker e registra handler de comandos
bool Initialize();

// Desliga o tracker e para thread
void Shutdown();

// Envia lista completa de objetos para o MuProxy2
void SendFullList();

// Envia apenas objetos vivos modificados (delta)
void SendDeltaList();

// Obtém referência para configurações atuais
TrackerConfig& GetConfig();

// Callback para PacketHandler
void OnCommand(const char* opcode, const char* subCmd,
               const uint8_t* data, int dataLen);

} // namespace MonsterTracker