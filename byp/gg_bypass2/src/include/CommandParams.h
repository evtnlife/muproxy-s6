#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// CommandParams — Parser dos parâmetros key=value dos comandos tipados
//
// Formato do payload (parte depois do primeiro '|' da linha "TAG|payload"):
//   "delay=500|range=8|skill=1E"
//
// Extraído do AttackModule (onde eram helpers file-local) para ser
// compartilhado por todos os módulos que registram comandos via
// MessageRegistry (AttackModule, DebugTools).
// ============================================================================

namespace CommandParams {

// Busca "key=" no payload e devolve o ponteiro para o valor (nullptr se não achar)
inline const char* Find(const char* payload, const char* key) {
    if (!payload || !key) return nullptr;
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char* p = strstr(payload, search);
    return p ? p + strlen(search) : nullptr;
}

// Lê um parâmetro inteiro (base 10). Ex.: delay=500
inline bool ParseIntParam(const char* payload, const char* key, int* outVal) {
    const char* p = Find(payload, key);
    if (!p) return false;
    *outVal = atoi(p);
    return true;
}

// Lê um parâmetro de 1 byte. Ex.: type=1
inline bool ParseByteParam(const char* payload, const char* key, uint8_t* outVal) {
    const char* p = Find(payload, key);
    if (!p) return false;
    *outVal = (uint8_t)atoi(p);
    return true;
}

// Lê um parâmetro float. Ex.: range=3.5
inline bool ParseFloatParam(const char* payload, const char* key, float* outVal) {
    const char* p = Find(payload, key);
    if (!p) return false;
    *outVal = (float)atof(p);
    return true;
}

// Lê um parâmetro WORD em hexadecimal. Ex.: skill=1E
inline bool ParseHexParam(const char* payload, const char* key, uint16_t* outVal) {
    const char* p = Find(payload, key);
    if (!p) return false;
    unsigned int val;
    if (sscanf_s(p, "%x", &val) == 1) {
        *outVal = (uint16_t)val;
        return true;
    }
    return false;
}

} // namespace CommandParams
