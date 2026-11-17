// packets/HwidRewrite.cpp — Reescreve o HWID no pacote de login (C1 F1)
//
// O servidor usa o HWID para limitar uma sessão por máquina. Este módulo
// troca o HWID do pacote de login por um valor por-instância antes de enviar.
//
// Derivação (determinística, sem estado):
//   base 0  -> HWID real (nada muda)
//   base>0  -> HWID real com o último DWORD XOR (base * 2654435761u)
// Override: MUPROXY_HWID=XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX força um valor.
//
// Fail-safe: qualquer falha => o pacote original é enviado sem alteração.
#include "HwidRewrite.h"
#include "MuCipher.h"
#include "SimpleModulusCipher.h"
#include "InstancePorts.h"
#include "Logger.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace HwidRewrite {

// HWID no formato "%08X-%08X-%08X-%08X" = 35 caracteres + '\0'.
static const int HWID_LEN = 35;
static const int HWID_BUF = HWID_LEN + 1;

static char g_OverrideHwid[HWID_BUF] = {0};
static bool g_OverrideValid = false;
static bool g_Initialized = false;

static bool IsHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// Valida e normaliza (maiúsculas) um HWID "XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX".
static bool NormalizeHwid(const char* in, char out[HWID_BUF])
{
    if (!in) return false;
    char buf[HWID_BUF];
    size_t n = strlen(in);
    if (n != (size_t)HWID_LEN) return false;
    for (int i = 0; i < HWID_LEN; i++) {
        char c = in[i];
        if (i == 8 || i == 17 || i == 26) {   // posições dos '-' (8, 17, 26)
            if (c != '-') return false;
            buf[i] = '-';
        } else {
            if (!IsHex(c)) return false;
            buf[i] = (c >= 'a' && c <= 'f') ? (char)(c - 32) : c;
        }
    }
    buf[HWID_LEN] = '\0';
    memcpy(out, buf, HWID_BUF);
    return true;
}

void Initialize()
{
    if (g_Initialized) return;
    g_Initialized = true;

    const char* env = getenv("MUPROXY_HWID");
    if (env && env[0]) {
        if (NormalizeHwid(env, g_OverrideHwid)) {
            g_OverrideValid = true;
            Logger::Log("HWID", "MUPROXY_HWID definido: %s (override ativo)", g_OverrideHwid);
        } else {
            Logger::Log("HWID", "MUPROXY_HWID invalido ('%s') — ignorado (formato esperado: XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX)", env);
        }
    }
    Logger::Log("HWID", "reescrita de HWID ativa (base=%d)%s",
                InstancePorts::Base(),
                g_OverrideValid ? " [override]" : "");
}

// Procura o padrão de HWID (35 bytes) em p[0..avail). Devolve o offset ou -1.
static int FindHwid(const unsigned char* p, int avail)
{
    for (int i = 0; i + HWID_LEN <= avail; i++) {
        bool ok = true;
        for (int j = 0; j < HWID_LEN; j++) {
            char c = (char)p[i + j];
            if (j == 8 || j == 17 || j == 26) {
                if (c != '-') { ok = false; break; }
            } else if (!IsHex(c)) {
                ok = false; break;
            }
        }
        if (ok) return i;
    }
    return -1;
}

// Deriva o HWID desta instância a partir do HWID real.
// base 0 -> idêntico ao real; base > 0 -> último DWORD XOR (base * 2654435761u).
static void DeriveHwid(const char* realHwid, unsigned int base, char out[HWID_BUF])
{
    unsigned int d[4] = {0, 0, 0, 0};
    if (sscanf_s(realHwid, "%8X-%8X-%8X-%8X", &d[0], &d[1], &d[2], &d[3]) != 4) {
        // Não deveria acontecer (o padrão já foi validado), mas fail-safe:
        memcpy(out, realHwid, HWID_BUF);
        return;
    }
    d[3] ^= base * 2654435761u;  // Knuth multiplicative hash
    snprintf(out, HWID_BUF, "%08X-%08X-%08X-%08X", d[0], d[1], d[2], d[3]);
}

// Decifra Layer-1 e devolve o pacote interno (C1...) em `inner`.
// Preenche innerLen, decKey (só no caminho C3) e isC3.
// Retorna false se não for um pacote que sabemos tratar.
static bool UnwrapInner(const char* buf, int len,
                        unsigned char* inner, int* innerLen,
                        unsigned char* decKey, bool* isC3)
{
    char tmp[2048];
    if (len > (int)sizeof(tmp)) return false;
    memcpy(tmp, buf, len);

    unsigned char xorKey = 0, subKey = 0;
    if (!MuCipher::GetGameServerKeys(&xorKey, &subKey)) return false;
    MuCipher::FullDecrypt(tmp, len, xorKey, subKey);   // Layer-1 decrypt in-place

    unsigned char c0 = (unsigned char)tmp[0];

    if (c0 == 0xC3) {
        const int hdr = 2;
        int smLen = len - hdr;
        if (smLen < SM_ENCRYPTED_SIZE) return false;

        unsigned char dec[2048];
        int decLen = SimpleModulusCipher::Decrypt(dec, tmp + hdr, smLen);
        if (decLen < 2) return false;

        *decKey = dec[0];
        int contentLen = decLen - 1;             // [F1][payload...]
        int total = 2 + contentLen;              // [C1][size][content]
        if (total > 255) return false;           // size byte é 1 byte
        if (total > (int)sizeof(tmp)) return false;

        inner[0] = 0xC1;
        inner[1] = (unsigned char)total;         // size byte (total <= 255)
        memcpy(inner + 2, dec + 1, contentLen);
        *innerLen = total;
        *isC3 = true;
        return true;
    }

    if (c0 == 0xC1) {
        if (len > 2048) return false;
        memcpy(inner, tmp, len);
        *innerLen = len;
        *decKey = 0;
        *isC3 = false;
        return true;
    }

    return false;
}

// Reconstrói o pacote de fio a partir do inner (já com XorData reaplicado).
static bool RebuildWire(const unsigned char* inner, int innerLen,
                        unsigned char decKey, bool isC3,
                        char* out, int outCap, int* outLen)
{
    unsigned char rebuilt[4096];
    int rebuiltLen = 0;

    if (isC3) {
        // SM encrypt de [decKey][inner[2..innerLen)]
        unsigned char smIn[2048];
        memset(smIn, 0, sizeof(smIn));
        smIn[0] = decKey;
        int contentLen = innerLen - 2;
        if (contentLen <= 0 || 1 + contentLen > (int)sizeof(smIn)) return false;
        memcpy(smIn + 1, inner + 2, contentLen);
        int smInLen = 1 + contentLen;

        unsigned char smOut[2048];
        int smOutLen = SimpleModulusCipher::Encrypt(smOut, smIn, smInLen);
        if (smOutLen <= 0) return false;

        rebuilt[0] = 0xC3;
        rebuilt[1] = (unsigned char)(2 + smOutLen);   // size byte = tamanho total
        if (2 + smOutLen > (int)sizeof(rebuilt)) return false;
        memcpy(rebuilt + 2, smOut, smOutLen);
        rebuiltLen = 2 + smOutLen;
    } else {
        if (innerLen > (int)sizeof(rebuilt)) return false;
        memcpy(rebuilt, inner, innerLen);
        rebuiltLen = innerLen;
    }

    // Layer-1 encrypt
    unsigned char xorKey = 0, subKey = 0;
    if (!MuCipher::GetGameServerKeys(&xorKey, &subKey)) return false;
    MuCipher::FullEncrypt((char*)rebuilt, rebuiltLen, xorKey, subKey);

    if (rebuiltLen > outCap) return false;
    memcpy(out, rebuilt, rebuiltLen);
    *outLen = rebuiltLen;
    return true;
}

bool RewriteIfLogin(const char* buf, int len, char* out, int outCap, int* outLen)
{
    if (!buf || !out || !outLen) return false;
    if (len < 8 || len > 2048) return false;
    if (outCap < len) return false;

    unsigned char inner[2048];
    int innerLen = 0;
    unsigned char decKey = 0;
    bool isC3 = false;

    if (!UnwrapInner(buf, len, inner, &innerLen, &decKey, &isC3)) return false;

    // Só pacotes de login: [C1][size][F1 ...]
    if (innerLen < 4) return false;
    if (inner[0] != 0xC1) return false;
    if (inner[2] != 0xF1) return false;

    // Decifra o Camada-2 do payload [3, innerLen)
    MuCipher::UnXorData((char*)inner, 3, innerLen);

    int hwidPos = FindHwid(inner + 3, innerLen - 3);
    if (hwidPos < 0) {
        MuCipher::XorData((char*)inner, 3, innerLen);  // restaura
        return false;
    }
    hwidPos += 3;

    char realHwid[HWID_BUF];
    memcpy(realHwid, inner + hwidPos, HWID_LEN);
    realHwid[HWID_LEN] = '\0';

    // Escolhe o HWID desta instância
    char newHwid[HWID_BUF];
    if (g_OverrideValid) {
        memcpy(newHwid, g_OverrideHwid, HWID_BUF);
    } else {
        DeriveHwid(realHwid, (unsigned int)InstancePorts::Base(), newHwid);
    }

    if (memcmp(realHwid, newHwid, HWID_LEN) == 0) {
        // Nada a mudar (base 0 sem override) — restaura e sai
        MuCipher::XorData((char*)inner, 3, innerLen);
        Logger::Log("HWID", "login: HWID real mantido (base=%d)", InstancePorts::Base());
        return false;
    }

    memcpy(inner + hwidPos, newHwid, HWID_LEN);

    // Reaplica o Camada-2
    MuCipher::XorData((char*)inner, 3, innerLen);

    // Reconstrói o pacote de fio
    int rebuiltLen = 0;
    if (!RebuildWire(inner, innerLen, decKey, isC3, out, outCap, &rebuiltLen)) {
        return false;
    }

    // Verificação fail-safe: decifra o que vamos enviar e confere o HWID novo.
    // Se não der para verificar, NÃO envia o pacote reescrito.
    {
        char verify[2048];
        bool ok = false;
        if (rebuiltLen <= (int)sizeof(verify)) {
            memcpy(verify, out, rebuiltLen);
            unsigned char xorKey = 0, subKey = 0;
            if (MuCipher::GetGameServerKeys(&xorKey, &subKey)) {
                MuCipher::FullDecrypt(verify, rebuiltLen, xorKey, subKey);
                if (isC3) {
                    int hdr = 2;
                    int smLen = rebuiltLen - hdr;
                    unsigned char dec2[2048];
                    int decLen2 = SimpleModulusCipher::Decrypt(dec2, verify + hdr, smLen);
                    if (decLen2 == (int)(innerLen - 1) && dec2[0] == decKey) {
                        // dec2[1..] deve ser igual a inner[2..innerLen) (já com XorData reaplicado)
                        if (memcmp(dec2 + 1, inner + 2, innerLen - 2) == 0) ok = true;
                    }
                } else {
                    if (rebuiltLen == innerLen && memcmp(verify, inner, innerLen) == 0) ok = true;
                }
            }
        }
        if (!ok) {
            Logger::Log("HWID", "verificacao falhou — pacote NAO alterado");
            return false;
        }
    }

    *outLen = rebuiltLen;
    Logger::Log("HWID", "login: HWID %s -> %s (base=%d)", realHwid, newHwid, InstancePorts::Base());
    return true;
}

} // namespace HwidRewrite
