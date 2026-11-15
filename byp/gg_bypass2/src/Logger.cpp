#include "include/Logger.h"
#include "include/Config.h"
#include "include/sdk.h"
#include <stdio.h>
#include <stdarg.h>
#include <share.h>

static FILE* g_LogFile = nullptr;  // arquivo unico: logs de sistema + pacotes
static CRITICAL_SECTION g_LogLock;
static volatile LONG g_LogInitialized = 0;

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
            fflush(g_LogFile);
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
        fflush(g_LogFile);
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
        fflush(g_LogFile);
    }

    LeaveCriticalSection(&g_LogLock);
}
