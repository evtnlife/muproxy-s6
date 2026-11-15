#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// AttackModule — Monta e envia pacotes de ataque para o game server
//
// Recebe comandos do MuProxy2 via TCP e monta pacotes MU Online cifrados
// para atacar monstros. Suporta ataque normal e ataque com skill.
//
// Comandos recebidos (via PacketHandler):
//
//   CMD ATTACK START <targetIndex> [delay=<ms>] [range=<float>] [skill=<hex>] [type=<0|1>]
//     → Inicia ataque contínuo no alvo.
//       targetIndex  = entity index (OFF_INDEX 0x07E) do alvo
//       O AttackTargetID é calculado internamente como: EntityIndex XOR 0xEFF5       (usado tanto em ataques normais 0x11 quanto skill attacks 0x19)//       delay        = intervalo entre ataques em ms (padrão: 800)
//       range        = distância máxima para atacar (padrão: 0 = sem limite)
//       skill        = ID da skill para ataque mágico (hex, ex: skill=1E)
//       type         = 0=normal, 1=skill (padrão: 0; se skill= definido, vira 1)
//
//   CMD ATTACK STOP
//     → Para ataque contínuo
//
//   CMD ATTACK ONCE <targetIndex> [skill=<hex>] [type=<0|1>]
//     → Ataca uma vez
//
//   CMD ATTACK CONFIG delay=<ms> range=<float>
//     → Altera configurações padrão do módulo
//
// Formatos de pacote:
//   Ataque normal:  C1 07 11 <targetHi> <targetLo> <action> <dir>
//   Ataque skill:   C1 07 19 <skill> <targetHi> <targetLo> <dir> (plaintext)
//                   → cifrado via main.exe+0x404D70 → C3 0D E5 <10 bytes>
//   AttackTargetID = EntityIndex XOR 0xEFF5 (big-endian no pacote) — ataques normais E skill
// ============================================================================

namespace AttackModule {

// Configurações dinâmicas do módulo (podem ser alteradas via TCP)
struct AttackConfig {
    int     defaultDelayMs;   // Intervalo entre ataques (ms)
    float   defaultRange;     // Distância máxima padrão (0 = sem limite)
    uint8_t defaultType;      // 0=normal, 1=skill
    uint16_t defaultSkillId;  // Skill ID padrão para type=1
    int     defaultHitCount;  // Quantos hits por mob por ciclo (padrão: 1)
};

// Inicializa o módulo e registra handler de comandos
bool Initialize();

// Desliga o módulo e para thread
void Shutdown();

// Envia pacote de ataque normal (opcode 0x11) para o target
// entityIndex é o OFF_INDEX (0x07E); XOR com 0xEFF5 é feito internamente
bool SendAttack(uint16_t entityIndex);

// Envia pacote de ataque com skill (opcode 0x19, TARGETED_SKILL)
// entityIndex é o OFF_INDEX (0x07E); XOR com 0xEFF5 é feito internamente
// para obter o AttackTargetID que vai no pacote, igual ao ataque normal.
bool SendSkillAttack(uint16_t entityIndex, uint16_t skillId);

// Envia ataque considerando tipo, skill, range e delay configurados
bool SendConfiguredAttack(int slotIndex);

// Obtém referência para configurações atuais
AttackConfig& GetConfig();

} // namespace AttackModule