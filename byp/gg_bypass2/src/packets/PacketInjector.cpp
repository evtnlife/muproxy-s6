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
//        ├── 0x19 PipelineTargetedSkill  → SM(serial+opcode+payload) → C3 0D [11] → Layer-1 → wire
//        └── 0x26 PipelineItemUse        → SM(serial+opcode+payload) → C3 0D [11] → Layer-1 → wire
//
// Ou seja: **skill (0x19) e uso de item (0x26) vão DENTRO de um C3+SM**;
// só o ataque normal (0x11) é C1 puro. Confirmado nas capturas do jogo:
//   skill  : C3 0D → inner C1 07 19 … → SM source = [serial] + 5 bytes (numBytes=6)
//   item   : C3 0D → inner C1 06 26 … → SM source = [serial] + 4 bytes (numBytes=5)
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

// ============================================================================
// Throttle do log detalhado da injeção (caminho quente).
//
// Com o auto-ataque ligado são centenas de injeções por segundo; cada uma
// escrevia ~4 linhas no log. Com fflush por linha isso saturava o disco e
// segurava o lock global do logger — o CLIENT TRAVAVA. O detalhe sai 1 a
// cada 20 injeções; a linha de confirmação ("sent C3 ...") sai sempre.
// ============================================================================
static volatile LONG g_InjectLogCounter = 0;
static inline bool ShouldLogInjectDetail() {
    return (InterlockedIncrement(&g_InjectLogCounter) % 20) == 1;
}

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

// ---- comum: empacota um C1 (payload JÁ com Camada-2) num C3+SM e envia -------
//
// Formato CONFIRMADO nas capturas do jogo (18:25:08 skill / 18:51:47 item):
//   inner C1 : C1 <size> <opcode> <payload...>
//   SM source: [serial 0x08793700] + innerC1[2..size]   (opcode + payload)
//   → SimpleModulus (size-1 bytes → 11) → C3 0D [11] → Layer-1 → wire
// Ou seja: o SM source tem SEMPRE (size-1) bytes e o numBytes do checksum
// acompanha (skill 0x19 → 6 bytes; uso de item 0x26 → 5 bytes).
//
// O serial (MAIN_PACKET_SERIAL) é lido e incrementado — o próximo C3 do jogo
// continua na sequência.
bool SendC3Sm(const uint8_t* buf, int len, const char* tag)
{
    const int smSrcLen = len - 2;          // opcode + payload (sem o C1 nem o size)
    if (smSrcLen < 1 || smSrcLen > SM_BLOCK_SIZE - 1) return false;

    // O buffer tem SM_BLOCK_SIZE (8) bytes de propósito: o EncryptBlock cifra
    // 8 bytes (o word 3 da chave SM sai dos bytes extras) e o checksum cobre OS
    // MESMOS 8 bytes. Com um array curto, os extras seriam lixo de stack: a
    // cifra usaria o lixo e o checksum usaria zeros → pacote autoinconsistente
    // → o servidor falha o checksum do SM e derruba a conexão.
    uint8_t* pSerial = (uint8_t*)0x08793700;            // MAIN_PACKET_SERIAL (abs)
    uint8_t smInput[SM_BLOCK_SIZE] = { 0 };
    smInput[0] = *pSerial;
    memcpy(smInput + 1, buf + 2, smSrcLen);
    (*pSerial)++;

    char smInHex[48];
    int ih = 0;
    for (int i = 0; i < smSrcLen + 1; i++)
        ih += snprintf(smInHex + ih, sizeof(smInHex) - ih, "%02X ", smInput[i]);
    const bool logDetail = ShouldLogInjectDetail();
    if (logDetail)
        Logger::Log("PACKETSNIFFER", "%s: SM input [%s] (serial consumido)", tag, smInHex);

    // SimpleModulus: (smSrcLen+1) bytes → 11 bytes (1 bloco)
    uint8_t smOutput[64];
    int smLen = SimpleModulusCipher::Encrypt(smOutput, smInput, smSrcLen + 1);
    if (smLen <= 0) {
        Logger::Log("PACKETSNIFFER", "%s: SM encrypt failed (smLen=%d)", tag, smLen);
        return false;
    }

    char smHex[128];
    int sh = 0;
    for (int i = 0; i < smLen && sh + 3 < (int)sizeof(smHex); i++)
        sh += snprintf(smHex + sh, sizeof(smHex) - sh, "%02X ", smOutput[i]);
    if (logDetail)
        Logger::Log("PACKETSNIFFER", "%s: SM encrypted [%s] len=%d", tag, smHex, smLen);

    // C3 <size> [SM ciphertext]
    uint8_t c3Buf[2 + 64];
    c3Buf[0] = (uint8_t)MuOpcodes::HeaderType::C3;
    c3Buf[1] = (uint8_t)(2 + smLen);
    memcpy(c3Buf + 2, smOutput, smLen);

    // Layer-1
    uint8_t wire[2 + 64];
    memcpy(wire, c3Buf, 2 + smLen);
    MuCipher::FullEncrypt((char*)wire, 2 + smLen, 0x36, 0x07);

    if (!g_Send || !g_Send(wire, 2 + smLen)) {
        Logger::Log("PACKETSNIFFER", "%s: send failed", tag);
        return false;
    }
    return true;
}

// ---- 0x26 — uso de item (consumível: potion) ------------------------------
// C1 06 26 <SourceSlot> <TargetSlot> <type> — vai DENTRO de um C3+SM, igual ao
// jogo (captura 18:51:47: C3 0D → inner C1 06 26 ... numBytes=5).
// O servidor (CharacterUsePotion) aplica HP/MP/SD conforme o item no slot.
bool PipelineItemUse(const uint8_t* buf, int len)
{
    // Log em claro: o buffer recebido está na forma Camada-2.
    uint8_t plain[6];
    memcpy(plain, buf, 6);
    MuCipher::UnXorData((char*)plain, 3, 6);

    Logger::Log("PACKETSNIFFER", "Inject: uso de item C1 26 [C1 06 26 %02X %02X %02X] (slot=%u)",
                plain[3], plain[4], plain[5], plain[3]);

    if (!SendC3Sm(buf, len, "InjectItemUse")) return false;

    Logger::Log("PACKETSNIFFER", "InjectItemUse: sent C3 0D (13 bytes) slot=%u target=0x%02X type=0x%02X",
                plain[3], plain[4], plain[5]);
    return true;
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
    // Log com os valores REAIS (skill/index em claro): o buffer recebido já
    // está na forma Camada-2, então decodifica-se uma cópia (Camada-2 inversa).
    uint8_t plain[7];
    memcpy(plain, buf, 7);
    MuCipher::UnXorData((char*)plain, 3, 7);
    const unsigned plainSkill = (plain[3] << 8) | plain[4];
    const unsigned plainIdx   = (plain[5] << 8) | plain[6];

    if (!SendC3Sm(buf, len, "InjectSkillC1")) return false;

    if (ShouldLogInjectDetail())
        Logger::Log("PACKETSNIFFER", "InjectSkillC1: sent C3 0D (13 bytes) skill=0x%04X index=0x%04X",
                    plainSkill, plainIdx);
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
            if (ShouldLogInjectDetail())
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
