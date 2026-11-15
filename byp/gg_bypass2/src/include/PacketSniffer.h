#pragma once
#include <cstdint>

namespace PacketSniffer {

// Skill packet types
enum SkillPacketType {
    SKILL_TYPE_STRUCT = 0,    // Direct struct: [word size][C1 xx F1 data...]
    SKILL_TYPE_BUILDER = 1,   // Use game builder functions (0x404B40 etc.)
    SKILL_TYPE_MANUAL = 2     // Manual: form C1 payload → SM encrypt → C3 → Layer-1 → send
};

bool Install();
void InstallHooksForAllModules();
void Uninstall();

// Injeta pacote direto no socket do game server.
// Recebe a forma ja processada pela Camada 2 (XorData) nos pacotes C1 0x11
// (ex.: a forma que o byp loga / o usuario copia do log) e aplica so Layer-1
// (XOR+sub) antes de enviar via send(). Para o plaintext REAL de um ataque
// C1 0x11, use InjectC1Attack (que aplica XorData + Layer-1).
bool InjectToServer(const uint8_t* data, int len);

// Injeta o PLAINTEXT REAL de um ataque C1 0x11 aplicando as DUAS camadas:
//   plaintext -> XorData[3,len) (Camada 2) -> Layer-1 (Camada 1) -> wire
// VALIDADO (RE, 4 capturas): C1 07 11 00 1A 78 04 -> FE 38 2E C0 C2 8B 48.
bool InjectC1Attack(const uint8_t* plaintext, int len);

// Fila thread-safe para skill attacks (C3).
// AttackModule enfileira o plaintext; HookSend processa na thread do jogo
// chamando SimpleModulus::Encrypt (0x998420) no contexto seguro.
// skillId: skill number (ex: 0x14 = Twisting Slash)
// attackId: RAW entity index (sem XOR 0xEFF5!) — o servidor MuEmu lê
//   bIndex = MAKE_NUMBERW(indexH, indexL) e OBJECT_RANGE(bIndex) requer < MAX_OBJECTS.
//   XOR 0xEFF5 é APENAS para ataques normais (opcode 0x11).
bool EnqueueSkillPacket(const uint8_t* plaintext, int len, SkillPacketType type = SKILL_TYPE_STRUCT,
                       uint16_t skillId = 0, uint16_t attackId = 0);

} // namespace PacketSniffer
