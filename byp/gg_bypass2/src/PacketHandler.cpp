// PacketHandler.cpp — Dispatcher de comandos recebidos do MuProxy2
// Agora delega para MessageRegistry (sistema de mensagens tipadas).
#include "include/PacketHandler.h"
#include "include/MessageRegistry.h"
#include "include/Logger.h"
#include "include/TcpClient.h"
#include "include/GameMemory.h"
#include "include/MuOpcodes.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace PacketHandler {

static const int MAX_HANDLERS = 16;
static const int MAX_OPCODE_LEN = 16;

struct HandlerEntry {
    char opcode[MAX_OPCODE_LEN];
    PacketCallback callback;
};

static HandlerEntry g_Handlers[MAX_HANDLERS];
static int g_HandlerCount = 0;

bool Initialize() {
    g_HandlerCount = 0;
    MessageRegistry::Initialize();
    Logger::Log("PACKETHANDLER", "Inicializado");
    return true;
}

void RegisterHandler(const char* opcode, PacketCallback callback) {
    if (!opcode || !callback || g_HandlerCount >= MAX_HANDLERS) return;

    for (int i = 0; i < g_HandlerCount; ++i) {
        if (strcmp(g_Handlers[i].opcode, opcode) == 0) {
            g_Handlers[i].callback = callback;
            Logger::Log("PACKETHANDLER", "Handler substituido: %s", opcode);
            return;
        }
    }

    strncpy_s(g_Handlers[g_HandlerCount].opcode, opcode, MAX_OPCODE_LEN - 1);
    g_Handlers[g_HandlerCount].opcode[MAX_OPCODE_LEN - 1] = '\0';
    g_Handlers[g_HandlerCount].callback = callback;
    g_HandlerCount++;
    Logger::Log("PACKETHANDLER", "Handler registrado: %s", opcode);
}

// Handler para o tag "CMD" — compatibilidade com handlers antigos (MonsterTracker)
// payload: "MONSTER|START|interval=500|filter=6"
static void HandleCmd(const char* payload) {
    char opcode[32] = {}, subCmd[32] = {};
    const char* p = payload;

    int i = 0;
    while (*p && *p != '|' && i < 31) opcode[i++] = *p++;
    if (*p == '|') p++;

    i = 0;
    while (*p && *p != '|' && i < 31) subCmd[i++] = *p++;
    if (*p == '|') p++;

    int dataLen = (int)strlen(p);

    for (int h = 0; h < g_HandlerCount; ++h) {
        if (strcmp(g_Handlers[h].opcode, opcode) == 0) {
            g_Handlers[h].callback(opcode, subCmd, (const uint8_t*)p, dataLen);
            return;
        }
    }
    Logger::Log("PACKETHANDLER", "CMD sem handler: %s", opcode);
}
REGISTER_MESSAGE(CMD, HandleCmd);

void ProcessLine(const char* line, int len) {
    if (!line || len <= 0) return;

    // DIAG
    {
        char tmp[300];
        int n = len < (int)sizeof(tmp) - 1 ? len : (int)sizeof(tmp) - 1;
        memcpy(tmp, line, n);
        tmp[n] = '\0';
        for (int i = 0; i < n; i++) if (tmp[i] == '\r' || tmp[i] == '\n') tmp[i] = ' ';
        Logger::Log("PACKETHANDLER", "RECV linha (%d): [%s]", n, tmp);
    }

    // Tenta MessageRegistry primeiro (sistema tipado TAG|payload)
    MessageRegistry::Dispatch(line, len);

    // Fallback: sistema antigo (PacketHandler::RegisterHandler)
    // Formato: OPCODE|SUBCMD|data
    char opcode[32] = {}, subCmd[32] = {};
    const char* p = line;
    int i = 0;
    while (*p && *p != '|' && i < 31) opcode[i++] = *p++;
    if (*p == '|') p++;
    i = 0;
    while (*p && *p != '|' && i < 31) subCmd[i++] = *p++;
    if (*p == '|') p++;
    int dataLen = (int)(line + len - p);
    if (dataLen < 0) dataLen = 0;

    for (int h = 0; h < g_HandlerCount; ++h) {
        if (strcmp(g_Handlers[h].opcode, opcode) == 0) {
            g_Handlers[h].callback(opcode, subCmd, (const uint8_t*)p, dataLen);
            return;
        }
    }
}

void Shutdown() {
    g_HandlerCount = 0;
    MessageRegistry::Shutdown();
    Logger::Log("PACKETHANDLER", "Desligado");
}

} // namespace PacketHandler