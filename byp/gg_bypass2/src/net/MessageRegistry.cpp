// MessageRegistry.cpp — Sistema de mensagens tipadas
#include "MessageRegistry.h"
#include "Logger.h"

#include <cstring>

namespace MessageRegistry {

static const int MAX_ENTRIES = 32;
static Entry g_Entries[MAX_ENTRIES];
static int g_EntryCount = 0;

void Register(const char* tag, MessageHandler handler) {
    if (!tag || !handler || g_EntryCount >= MAX_ENTRIES) return;

    // Substitui handler existente com mesmo tag
    for (int i = 0; i < g_EntryCount; ++i) {
        if (strcmp(g_Entries[i].tag, tag) == 0) {
            g_Entries[i].handler = handler;
            Logger::Log("MSGREG", "Handler substituido: %s", tag);
            return;
        }
    }

    g_Entries[g_EntryCount].tag = tag;
    g_Entries[g_EntryCount].handler = handler;
    g_EntryCount++;
    Logger::Log("MSGREG", "Handler registrado: %s", tag);
}

void Dispatch(const char* line, int len) {
    if (!line || len <= 0) return;

    // Encontra o primeiro '|'
    const char* sep = nullptr;
    for (int i = 0; i < len; ++i) {
        if (line[i] == '|') { sep = line + i; break; }
    }

    int tagLen;
    const char* payload;
    if (sep) {
        tagLen = (int)(sep - line);
        payload = sep + 1;
    } else {
        // Linha sem payload: a tag e a linha inteira (ex.: "POTION_STATUS").
        tagLen = len;
        payload = line + len;
    }

    // Busca linear — poucos tipos, não vale hash map
    for (int i = 0; i < g_EntryCount; ++i) {
        if (strncmp(g_Entries[i].tag, line, tagLen) == 0 && g_Entries[i].tag[tagLen] == '\0') {
            g_Entries[i].handler(payload);
            return;
        }
    }

    Logger::Log("MSGREG", "Tag desconhecida: %.*s", tagLen, line);
}

void Initialize() {
    // Não zera g_EntryCount — os handlers já foram registrados
    // pelas macros REGISTER_MESSAGE durante a inicialização estática.
    Logger::Log("MSGREG", "Inicializado (%d handlers)", g_EntryCount);
}

void Shutdown() {
    g_EntryCount = 0;
    Logger::Log("MSGREG", "Desligado");
}

} // namespace MessageRegistry
