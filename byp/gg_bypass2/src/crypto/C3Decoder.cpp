// crypto/C3Decoder.cpp — Decifra e loga o C1/C2 interno de um C3/C4
//
// Extraído do WinsockHooks (Fase 6/9 do refactor reorganize-byp-modules).
#include "C3Decoder.h"
#include "SimpleModulusCipher.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>

namespace C3Decoder {

static InnerPacketObserver g_InnerObserver = nullptr;

void SetInnerPacketObserver(InnerPacketObserver obs)
{
    g_InnerObserver = obs;
}

void LogDecoded(const char* displayBuf, int displayLen)
{
    if (!displayBuf || displayLen <= 2) return;
    if ((unsigned char)displayBuf[0] != 0xC3 && (unsigned char)displayBuf[0] != 0xC4) return;

    uint8_t pktHdr = (unsigned char)displayBuf[0];
    int hdrSize = (pktHdr == 0xC3) ? 2 : 3;  // C3=2 bytes header, C4=3 bytes header
    int smLen = displayLen - hdrSize;
    if (smLen < SM_ENCRYPTED_SIZE) return;

    // Log the exact bytes being passed to SM decrypt for debugging
    {
        char smHex[128];
        int sp = 0;
        int sblen = smLen > 22 ? 22 : smLen;
        for (int j = 0; j < sblen && sp < (int)sizeof(smHex) - 4; ++j)
            sp += snprintf(smHex + sp, sizeof(smHex) - sp, "%02X ", (unsigned char)displayBuf[hdrSize+j]);
        Logger::Log("SMOD", "SM input bytes (%d): [%s]", smLen, smHex);
    }

    uint8_t decrypted[256];
    bool decryptedOk = false;

    int decLen = SimpleModulusCipher::Decrypt(decrypted, displayBuf + hdrSize, smLen);
    if (decLen > 0) {
        decryptedOk = true;
        Logger::Log("PACKETSNIFFER", "SEND C3 decrypt: OK decLen=%d", decLen);
    } else {
        Logger::Log("PACKETSNIFFER", "SEND C3 decrypt: FAILED (decLen=%d)", decLen);
    }

    if (!decryptedOk) return;

    // C3/C4 SM decrypt format:
    //   stream[0] = decKey (encryption key byte, not part of payload)
    //   stream[1..validTotal-1] = C1/C2 payload (plaintext)
    //
    // BlackRock NÃO usa XOR C1C2 (tabela 0xE7,0x6D...):
    //   - Evidência: C1 direto "C1 08 D4 02" (movimento) aparece com
    //     opcode D4 após Layer-1 decrypt. Se XOR C1C2 fosse usado,
    //     D4 seria codificado para 8A (calculado via Python).
    //   - Evidência: C3 decrypt sem XOR dá opcode 0x19 (attack) correto.
    //   - Evidência: MuProxy define EnableXorDecode=false por padrão.
    //   - MuEncDec.DecodeC3C4 chama DecodeC1C2InPlace, mas isso é
    //     desabilitado via DisableC1C2Xor=true no BlackRock.
    //   - XorFilter (tabela 0xAB,0x11...) = Camada 2: NÃO usado no
    //     caminho C3C4 (que usa SimpleModulus), MAS usado nos pacotes
    //     de ataque C1 0x11 (ver PacketInjector::Inject / RE validada).
    //
    // Reconstruct: C3→C1, C4→C2
    //   innerPkt = [C1/C2] [size] [stream[1]] ... [stream[validTotal-1]]

    uint8_t hdr = (uint8_t)displayBuf[0];
    uint8_t innerHdr = (hdr == 0xC4) ? 0xC2 : 0xC1;
    int innerHdrSize = (innerHdr == 0xC2) ? 3 : 2;
    int contentBytes = decLen - 1;  // skip decKey byte
    int innerTotalSize = innerHdrSize + contentBytes;

    if (contentBytes <= 0) {
        Logger::Log("PACKETSNIFFER", "SEND C3 decrypt: empty payload (decLen=%d)", decLen);
        return;
    }

    uint8_t innerPkt[256];
    int pos = 0;
    innerPkt[pos++] = innerHdr;
    if (innerHdrSize == 2) {
        innerPkt[pos++] = (uint8_t)innerTotalSize;
    } else {
        innerPkt[pos++] = (uint8_t)((innerTotalSize >> 8) & 0xFF);
        innerPkt[pos++] = (uint8_t)(innerTotalSize & 0xFF);
    }
    // Copy stream[1..] as payload (skip decKey at stream[0])
    memcpy(innerPkt + pos, decrypted + 1, contentBytes);

    // Log the decoded C1/C2 packet
    char decHex[256];
    int dp = 0;
    for (int j = 0; j < innerTotalSize && dp < (int)sizeof(decHex) - 4; ++j)
        dp += snprintf(decHex + dp, sizeof(decHex) - dp, "%02X ", innerPkt[j]);
    Logger::Log("PACKETSNIFFER", "SEND %s decoded: [%s] dk=%02X",
        (innerHdr == 0xC2) ? "C4>C2" : "C3>C1", decHex, decrypted[0]);

    // Entrega o pacote interno aos interessados (ex.: o auto-potion, que
    // reconhece o uso de item 0x26 aqui — o C3+SM do cliente).
    if (g_InnerObserver)
        g_InnerObserver(innerPkt, innerTotalSize);
}

} // namespace C3Decoder
