// NameRewriter.cpp — Rewrite de nome/classe no pacote de create-char (0x31)
//
// Extraído do PacketSniffer (Fase 3 do refactor reorganize-byp-modules).
#include "include/NameRewriter.h"

#include <cstring>

namespace NameRewriter {

const BYTE g_NameKey[10] = { 0xCF, 0xDB, 0x23, 0xC5, 0xA3, 0xCA, 0x33, 0xC1, 0xCC, 0x66 };
static const BYTE g_ClassKey = 0x63;

// Nome e classe desejados
const char* g_RewriteName = "HexDL";
const BYTE  g_RewriteClass = 0x10; // Dark Lord (client-side)

void EncodeName(const char* name, BYTE classId, BYTE* outEncoded)
{
    int nameLen = (int)strlen(name);
    for (int i = 0; i < 10; i++) {
        BYTE p = (i < nameLen) ? (BYTE)name[i] : 0;
        if (i == 0)
            outEncoded[i] = p ^ g_NameKey[0];
        else if (i == 1)
            outEncoded[i] = p ^ g_NameKey[1];
        else
            outEncoded[i] = p ^ outEncoded[i-1] ^ g_NameKey[i];
    }
    outEncoded[10] = classId ^ outEncoded[8] ^ g_ClassKey;
}

bool IsCreateCharPacket(const char* data, int len)
{
    if (len < 15) return false;
    return (BYTE)data[0] == 0xC1 && (BYTE)data[1] == 0x0F
        && (BYTE)data[2] == 0x31 && (BYTE)data[3] == 0xC3;
}

} // namespace NameRewriter
