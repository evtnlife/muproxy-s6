// packets/PacketDiag.cpp — arquivo de diagnóstico do tráfego S→C
//
// Ver include/PacketDiag.h para o porquê.
#include "PacketDiag.h"
#include "Logger.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace PacketDiag {

static FILE* g_File = nullptr;
static unsigned g_OpCount[256];      // histograma de opcodes S→C
static unsigned g_PacketCount = 0;
static int      g_FlushCounter = 0;

// Quantos pacotes S→C gravar em detalhe (hex) no arquivo de diagnóstico.
static const int MAX_DETAIL_PACKETS = 400;

static void WriteRaw(const char* text, int len)
{
    if (!g_File) return;
    fwrite(text, 1, (size_t)len, g_File);
    if (++g_FlushCounter >= 32) {
        fflush(g_File);
        g_FlushCounter = 0;
    }
}

bool Initialize(const char* dirPath)
{
    char path[MAX_PATH] = { 0 };
    if (dirPath && dirPath[0]) {
        strncpy_s(path, dirPath, MAX_PATH - 1);
        size_t n = strlen(path);
        if (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/')
            strcat_s(path, "\\");
    }
    strcat_s(path, sizeof(path), "gg_bypass2_s2c.log");

    if (g_File) { fclose(g_File); g_File = nullptr; }
    g_File = fopen(path, "w");     // "w" = TRUNCA (diagnóstico limpo a cada boot)
    if (!g_File) return false;

    memset(g_OpCount, 0, sizeof(g_OpCount));
    g_PacketCount = 0;
    g_FlushCounter = 0;

    fprintf(g_File, "=== DIAGNOSTICO S->C (byp.dll) ===\n");
    fflush(g_File);
    return true;
}

void LogPacket(const char* dir, const uint8_t* data, int len)
{
    if (!data || len <= 0) return;

    const int hdr = (data[0] == 0xC1 || data[0] == 0xC3) ? 2 :
                    (data[0] == 0xC2 || data[0] == 0xC4) ? 3 : 0;
    const int opcode = (hdr > 0 && len > hdr) ? (unsigned char)data[hdr] : 0;

    if (dir && dir[0] == 'R') {
        g_OpCount[opcode & 0xFF]++;
        g_PacketCount++;
    }

    if (g_PacketCount > MAX_DETAIL_PACKETS) return;

    char line[1024];
    int n = snprintf(line, sizeof(line), "[%s] #%u hdr=%d op=%02X len=%d :",
                      dir ? dir : "?", g_PacketCount, hdr, opcode & 0xFF, len);
    if (n < 0) return;
    if (n > (int)sizeof(line) - 8) n = (int)sizeof(line) - 8;

    // O pacote INTEIRO: os C3 com SimpleModulus precisam do corpo todo para
    // decodificar (o hex truncado quebra o cálculo dos blocos).
    const int show = (len < 256) ? len : 256;
    for (int i = 0; i < show && n < (int)sizeof(line) - 4; i++)
        n += snprintf(line + n, sizeof(line) - n, "%02X ", data[i]);
    if (n < (int)sizeof(line) - 4)
        n += snprintf(line + n, sizeof(line) - n, "\n");

    WriteRaw(line, n);
}

void LogLine(const char* fmt, ...)
{
    if (!g_File || !fmt) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
    va_end(args);
    if (n <= 0) return;
    buf[n] = '\n';
    WriteRaw(buf, n + 1);
}

void LogHex(const char* tag, const uint8_t* data, int len)
{
    if (!data || len <= 0 || !g_File) return;
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[%s] len=%d :", tag ? tag : "?", len);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 8) n = (int)sizeof(buf) - 8;
    const int show = (len < 64) ? len : 64;
    for (int i = 0; i < show && n < (int)sizeof(buf) - 4; i++)
        n += snprintf(buf + n, sizeof(buf) - n, " %02X", data[i]);
    if (n < (int)sizeof(buf) - 4)
        n += snprintf(buf + n, sizeof(buf) - n, "\n");
    WriteRaw(buf, n);
}

void LogHistogram()
{
    // Log principal (sempre) + arquivo de diagnóstico.
    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
                     "PACKETSNIFFER: S->C total=%u | opcodes:",
                     g_PacketCount);
    for (int i = 0; i < 256 && n < (int)sizeof(buf) - 24; i++) {
        if (!g_OpCount[i]) continue;
        n += snprintf(buf + n, sizeof(buf) - n, " %02X=%u", i, g_OpCount[i]);
    }
    if (n < (int)sizeof(buf) - 4)
        snprintf(buf + n, sizeof(buf) - n, "\n");

    Logger::Log("PACKETSNIFFER", "%s", buf);
    WriteRaw(buf, (int)strlen(buf));
    fflush(g_File);
}

void Shutdown()
{
    if (!g_File) return;
    LogHistogram();
    fclose(g_File);
    g_File = nullptr;
}

} // namespace PacketDiag
