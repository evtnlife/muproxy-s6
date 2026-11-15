#pragma once
#include "sdk.h"

// ============================================================================
// Logger — Log simples em arquivo para debug/diagnostico
// ============================================================================
namespace Logger {
    void Initialize(const char* filePath, const char* packetLogPath);
    void Shutdown();
    void Log(const char* tag, const char* fmt, ...);
    void LogPacket(const char* tag, const char* dir, const char* data, int len);
    // Loga pacote com versao crypted (raw) e decrypted na mesma linha:
    //   hex=<raw> dec=<decrypted>
    // Se dec==nullptr ou decLen<=0, so loga hex=<raw> (comportamento antigo).
    void LogPacketDual(const char* tag, const char* dir, const char* raw, int rawLen, const char* dec, int decLen);
}
