#include "Logger.h"
#include "Config.h"
#include "sdk.h"
#include <stdio.h>
#include <stdarg.h>
#include <share.h>

static FILE* g_LogFile = nullptr;  // arquivo unico: logs de sistema + pacotes
static CRITICAL_SECTION g_LogLock;
static volatile LONG g_LogInitialized = 0;
static DWORD g_LastFlush = 0;

// ============================================================================
// Flush automático: no máximo 1x por segundo.
//
// O logger é chamado DENTRO do hook de rede (thread do jogo) e o lock é
// global. Com fflush() em cada linha, um módulo de ataque spameando milhares
// de linhas por segundo segurava o lock do log e o CLIENT TRAVAVA (a thread do
// jogo ficava esperando o CriticalSection). Além disso, cada fflush é uma
// escrita síncrona no disco.
//
// Agora o log fica no buffer do CRT e é gravado no máximo 1x/s (e sempre no
// Shutdown).
// ============================================================================
static const DWORD FLUSH_INTERVAL_MS = 1000;

static void MaybeFlushLocked()
{
    const DWORD now = GetTickCount();
    if (g_LastFlush == 0 || now - g_LastFlush >= FLUSH_INTERVAL_MS) {
        g_LastFlush = now;
        if (g_LogFile) fflush(g_LogFile);
    }
}

void Logger::Initialize(const char* filePath) {
    if (InterlockedCompareExchange(&g_LogInitialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_LogLock);
    }

    if (!Config::LOG_TO_FILE) return;  // não abre arquivos se log em disco desativado

    EnterCriticalSection(&g_LogLock);
    if (g_LogFile) {
        fclose(g_LogFile);
        g_LogFile = nullptr;
    }
    g_LogFile = _fsopen(filePath, "a", _SH_DENYNO);
    g_LastFlush = GetTickCount();
    LeaveCriticalSection(&g_LogLock);
}

void Logger::Shutdown() {
    if (g_LogInitialized == 0) return;

    EnterCriticalSection(&g_LogLock);
    if (g_LogFile) {
        fclose(g_LogFile);
        g_LogFile = nullptr;
    }
    LeaveCriticalSection(&g_LogLock);

    DeleteCriticalSection(&g_LogLock);
    InterlockedExchange(&g_LogInitialized, 0);
}

void Logger::Log(const char* tag, const char* fmt, ...) {
    if (g_LogInitialized == 0) return;
    if (!Config::LOG_TO_FILE) return;  // log em disco desativado

    EnterCriticalSection(&g_LogLock);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char fullMsg[4096];
    int prefixLen = snprintf(fullMsg, sizeof(fullMsg), "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, tag);

    va_list args;
    va_start(args, fmt);
    int msgLen = vsnprintf(fullMsg + prefixLen, sizeof(fullMsg) - prefixLen, fmt, args);
    va_end(args);

    int totalLen = prefixLen + msgLen;
    if (totalLen > 0 && totalLen < (int)sizeof(fullMsg))
    {
        if (g_LogFile) {
            fprintf(g_LogFile, "%s\n", fullMsg);
            MaybeFlushLocked();
        }
    }

    LeaveCriticalSection(&g_LogLock);
}

void Logger::LogPacket(const char* tag, const char* dir, const char* data, int len) {
    if (g_LogInitialized == 0 || !data || len <= 0) return;
    if (!Config::LOG_TO_FILE) return;  // log em disco desativado

    EnterCriticalSection(&g_LogLock);

    SYSTEMTIME st;
    GetLocalTime(&st);

    if (g_LogFile) {
        fprintf(g_LogFile, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s] len=%d hex=",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            tag, dir, len);
        for (int i = 0; i < len; ++i)
            fprintf(g_LogFile, "%02X", (unsigned char)data[i]);
        fprintf(g_LogFile, "\n");
        MaybeFlushLocked();
    }

    LeaveCriticalSection(&g_LogLock);
}

void Logger::LogPacketDual(const char* tag, const char* dir, const char* raw, int rawLen, const char* dec, int decLen) {
    if (g_LogInitialized == 0 || !raw || rawLen <= 0) return;
    if (!Config::LOG_TO_FILE) return;  // log em disco desativado

    EnterCriticalSection(&g_LogLock);

    SYSTEMTIME st;
    GetLocalTime(&st);

    if (g_LogFile) {
        // Formato antigo: hex=<raw wire> dec=<decodificado>
        fprintf(g_LogFile, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s] len=%d hex=",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            tag, dir, rawLen);
        for (int i = 0; i < rawLen; ++i)
            fprintf(g_LogFile, "%02X", (unsigned char)raw[i]);
        if (dec && decLen > 0) {
            fprintf(g_LogFile, " dec=");
            for (int i = 0; i < decLen; ++i)
                fprintf(g_LogFile, "%02X", (unsigned char)dec[i]);
        }
        fprintf(g_LogFile, "\n");
        MaybeFlushLocked();
    }

    LeaveCriticalSection(&g_LogLock);
}
