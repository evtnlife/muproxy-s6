// PacketInjector.cpp — Porta única de injeção no game server
//
// Extraído do PacketSniffer (Fase 4 do refactor reorganize-byp-modules, D1).
//
// As mensagens de log mantêm o prefixo "InjectSkillC1:" (o nome da função
// antiga) para preservar a continuidade com os docs de RE — o critério de
// verificação em campo é a linha "InjectSkillC1: after XorData [...]".
#include "PacketInjector.h"
#include "MuCipher.h"
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
// Pipeline do skill (0x19):
//   1. SM input      — [serial 0x08793700] + payload XorData'd [19 sH sL iH iL]
//   2. SimpleModulus — 6 bytes → 11 bytes (1 bloco)
//   3. C3 0D [11]    — header C3 + size
//   4. Layer-1       — XOR+sub
// O serial (MAIN_PACKET_SERIAL) é lido e incrementado — o próximo C3 do jogo
// continua na sequência.
// ============================================================================
static bool InjectSkillPipeline(const uint8_t* buf, int len)
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
    c3Buf[0] = 0xC3;
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

// ============================================================================
// Porta única: XorData uma vez + pipeline pelo opcode.
// ============================================================================
bool Inject(const uint8_t* plaintext, int len)
{
    if (!plaintext || len < 7) return false;
    if (plaintext[0] != 0xC1) return false;

    BYTE opcode = plaintext[2];
    if (opcode != 0x11 && opcode != 0x19) return false;
    // O skill tem formato fixo: C1 07 19 [skillH][skillL][indexH][indexL]
    if (opcode == 0x19 && len != 7) return false;

    uint8_t buf[4096];
    if (len > (int)sizeof(buf)) return false;
    memcpy(buf, plaintext, len);

    // Camada 2 (XorData): XOR encadeado nos bytes de dado [3, len).
    // Append 0x404C10 -> XorData 0x404C90 (flag=1) no builder do jogo.
    MuCipher::XorData((char*)buf, 3, len);

    if (opcode == 0x19) {
        char xorHex[32];
        int xh = 0;
        for (int i = 0; i < 7; i++)
            xh += snprintf(xorHex + xh, sizeof(xorHex) - xh, "%02X ", buf[i]);
        Logger::Log("PACKETSNIFFER", "InjectSkillC1: after XorData [%s]", xorHex);

        return InjectSkillPipeline(buf, len);
    }

    // 0x11: Layer-1 (XOR+sub) + send (C1 puro, sem SimpleModulus)
    uint8_t wire[4096];
    memcpy(wire, buf, len);
    MuCipher::FullEncrypt((char*)wire, len, 0x36, 0x07);

    if (!g_Send) {
        Logger::Log("PACKETSNIFFER", "Inject: sender nao registrado");
        return false;
    }
    return g_Send(wire, len);
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
