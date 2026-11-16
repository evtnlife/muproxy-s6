// packets/PacketInjector.cpp — Porta única de injeção no game server
//
// Estrutura (um pipeline por opcode):
//
//   Inject(plaintext, len)
//        │  valida header + tamanho pelo opcode
//        │  MuCipher::XorData(buf, 3, len)      ← Camada-2, UMA vez, para todos
//        ▼
//   switch (opcode)
//        ├── 0x11 PipelineAttackNormal   → Layer-1 → wire (C1 puro)
//        ├── 0x19 PipelineTargetedSkill  → SM(serial+payload) → C3 0D [11] → Layer-1 → wire
//        └── 0x26 PipelineItemUse        → Layer-1 → wire (C1 puro)
//
// As mensagens de log mantêm o prefixo "InjectSkillC1:" (o nome da função
// antiga) para preservar a continuidade com os docs de RE — o critério de
// verificação em campo é a linha "InjectSkillC1: after XorData [...]".
#include "PacketInjector.h"
#include "MuCipher.h"
#include "MuOpcodes.h"
#include "SimpleModulusCipher.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>

namespace PacketInjector {

static SendFn g_Send = nullptr;

void SetSender(SendFn fn)
{
    g_Send = fn;
}

// ============================================================================
// Pipelines (internos) — um por opcode
// ============================================================================
namespace {

// ---- comum: Layer-1 + envio no socket do game server ----------------------
// Recebe o payload JÁ com a Camada-2 aplicada; devolve o que foi enviado.
bool SendLayer1(const uint8_t* buf, int len)
{
    uint8_t wire[4096];
    memcpy(wire, buf, len);
    MuCipher::FullEncrypt((char*)wire, len, 0x36, 0x07);

    if (!g_Send) {
        Logger::Log("PACKETSNIFFER", "Inject: sender nao registrado");
        return false;
    }
    return g_Send(wire, len);
}

// ---- 0x26 — uso de item (consumível: potion) ------------------------------
// C1 06 26 <SourceSlot> <TargetSlot> <type>
// O servidor (CharacterUsePotion) aplica HP/MP/SD conforme o item no slot.
bool PipelineItemUse(const uint8_t* buf, int len)
{
    Logger::Log("PACKETSNIFFER", "Inject: uso de item C1 26 [C1 06 26 %02X %02X %02X]",
                buf[3], buf[4], buf[5]);
    return SendLayer1(buf, len);
}

// ---- 0x11 — ataque normal --------------------------------------------------
// C1 07 11 <targetHi> <targetLo> 0x78 <dir 0..7> → C1 puro (sem SimpleModulus).
bool PipelineAttackNormal(const uint8_t* buf, int len)
{
    return SendLayer1(buf, len);
}

// ---- 0x19 — ataque com skill ----------------------------------------------
// Pipeline do skill (0x19):
//   1. SM input      — [serial 0x08793700] + payload XorData'd [19 sH sL iH iL]
//   2. SimpleModulus — 6 bytes → 11 bytes (1 bloco)
//   3. C3 0D [11]    — header C3 + size
//   4. Layer-1       — XOR+sub
// O serial (MAIN_PACKET_SERIAL) é lido e incrementado — o próximo C3 do jogo
// continua na sequência.
bool PipelineTargetedSkill(const uint8_t* buf, int len)
{
    // 1. SM input = [serial] + payload XorData'd (5 bytes: 19 sH sL iH iL)
    uint8_t* pSerial = (uint8_t*)0x08793700;            // MAIN_PACKET_SERIAL (abs)
    uint8_t smInput[6];
    smInput[0] = *pSerial;
    memcpy(smInput + 1, buf + 2, 5);
    (*pSerial)++;

    char smInHex[48];
    int ih = 0;
    for (int i = 0; i < 6; i++)
        ih += snprintf(smInHex + ih, sizeof(smInHex) - ih, "%02X ", smInput[i]);
    Logger::Log("PACKETSNIFFER", "InjectSkillC1: SM input [%s] (serial consumido)", smInHex);

    // 2. SimpleModulus: 6 bytes → 11 bytes
    uint8_t smOutput[64];
    int smLen = SimpleModulusCipher::Encrypt(smOutput, smInput, 6);
    if (smLen <= 0) {
        Logger::Log("PACKETSNIFFER", "InjectSkillC1: SM encrypt failed (smLen=%d)", smLen);
        return false;
    }

    char smHex[128];
    int sh = 0;
    for (int i = 0; i < smLen && sh + 3 < (int)sizeof(smHex); i++)
        sh += snprintf(smHex + sh, sizeof(smHex) - sh, "%02X ", smOutput[i]);
    Logger::Log("PACKETSNIFFER", "InjectSkillC1: SM encrypted [%s] len=%d", smHex, smLen);

    // 3. C3 0D [11 bytes]
    uint8_t c3Buf[2 + 64];
    c3Buf[0] = (uint8_t)MuOpcodes::HeaderType::C3;
    c3Buf[1] = (uint8_t)(2 + smLen);
    memcpy(c3Buf + 2, smOutput, smLen);

    // 4. Layer-1
    uint8_t wire[2 + 64];
    memcpy(wire, c3Buf, 2 + smLen);
    MuCipher::FullEncrypt((char*)wire, 2 + smLen, 0x36, 0x07);

    // 5. send
    if (!g_Send || !g_Send(wire, 2 + smLen)) {
        Logger::Log("PACKETSNIFFER", "InjectSkillC1: send failed");
        return false;
    }

    Logger::Log("PACKETSNIFFER", "InjectSkillC1: sent C3 0D (%d bytes) skill=0x%04X index=0x%04X",
                2 + smLen, (buf[3] << 8) | buf[4], (buf[5] << 8) | buf[6]);
    return true;
}

} // namespace

// ============================================================================
// Porta única: XorData uma vez + pipeline pelo opcode.
// ============================================================================
bool Inject(const uint8_t* plaintext, int len)
{
    if (!plaintext || len < 6) return false;
    if (plaintext[0] != (uint8_t)MuOpcodes::HeaderType::C1) return false;

    const uint8_t opcode = plaintext[2];

    // Validação do tamanho por opcode (o payload do 0x26 tem 6 bytes).
    switch ((MuOpcodes::ClientToServer)opcode) {
        case MuOpcodes::ClientToServer::Attack:
            if (len < MuOpcodes::Size::AttackNormal) return false;
            break;
        case MuOpcodes::ClientToServer::TargetedSkill:
            if (len != MuOpcodes::Size::TargetedSkill) return false;
            break;
        case MuOpcodes::ClientToServer::ItemUse:
            if (len != MuOpcodes::Size::ItemUse) return false;
            break;
        default:
            return false;
    }

    uint8_t buf[4096];
    if (len > (int)sizeof(buf)) return false;
    memcpy(buf, plaintext, len);

    // Camada 2 (XorData): XOR encadeado nos bytes de dado [3, len).
    // Append 0x404C10 -> XorData 0x404C90 (flag=1) no builder do jogo.
    // O servidor decodifica [3,size) em ordem DECRESCENTE (SocketManager ->
    // PacketManager::ExtractPacket) — vale para TODO pacote C1, inclusive o 0x26.
    MuCipher::XorData((char*)buf, 3, len);

    switch ((MuOpcodes::ClientToServer)opcode) {
        case MuOpcodes::ClientToServer::Attack:
            return PipelineAttackNormal(buf, len);
        case MuOpcodes::ClientToServer::TargetedSkill: {
            char xorHex[32];
            int xh = 0;
            for (int i = 0; i < 7; i++)
                xh += snprintf(xorHex + xh, sizeof(xorHex) - xh, "%02X ", buf[i]);
            Logger::Log("PACKETSNIFFER", "InjectSkillC1: after XorData [%s]", xorHex);
            return PipelineTargetedSkill(buf, len);
        }
        case MuOpcodes::ClientToServer::ItemUse:
            return PipelineItemUse(buf, len);
        default:
            return false;
    }
}

// ============================================================================
// Injeção de dados JÁ na forma XorData (comando SEND_PACKET / "SEND <hex>"):
// aplica apenas a Layer-1 e envia.
// ============================================================================
bool InjectXorData(const uint8_t* data, int len)
{
    if (!data || len <= 0) return false;

    uint8_t wire[4096];
    if (len > (int)sizeof(wire)) return false;
    memcpy(wire, data, len);
    MuCipher::FullEncrypt((char*)wire, len, 0x36, 0x07);

    if (!g_Send) {
        Logger::Log("PACKETSNIFFER", "Inject: sender nao registrado");
        return false;
    }
    return g_Send(wire, len);
}

} // namespace PacketInjector
