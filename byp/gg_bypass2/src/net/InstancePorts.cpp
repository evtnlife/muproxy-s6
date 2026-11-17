// net/InstancePorts.cpp — Base de portas por instância (MUPROXY_BASE)
#include "sdk.h"
#include <cstdlib>
#include <cstring>

#include "InstancePorts.h"
#include "Config.h"
#include "Logger.h"

namespace InstancePorts {

// Índice do loopback por instância: base 0 -> 127.0.0.1, base 100 ->
// 127.0.0.2, ... O Windows escuta em todo o 127.0.0.0/8, então cada
// instância escuta nas MESMAS portas do servidor (44411 e a porta do GS),
// só que em um loopback diferente.
static const int MAX_LOOPBACK_INDEX = 9;

static int  g_Base = -1;      // -1 = ainda não resolvido
static int  g_CmdPort = 0;
static int  g_PktPort = 0;
static bool g_HasBase = false; // MUPROXY_BASE chegou neste processo?
static char g_SessionDir[MAX_PATH] = {0};
static char g_LocalIp[16] = {0};

static void Resolve() {
    if (g_Base >= 0) return;

    g_Base = 0;

    char buf[32] = {0};
    DWORD n = GetEnvironmentVariableA("MUPROXY_BASE", buf, sizeof(buf) - 1);
    if (n > 0 && n < sizeof(buf)) {
        int b = atoi(buf);
        // base 0 é válida (a primeira instância) — o que separa "base 0
        // explícita" de "sem instância" é a flag g_HasBase.
        if (b >= 0 && b <= 9000) { g_Base = b; g_HasBase = true; }
    }

    g_CmdPort = Config::TCP_PORT + g_Base;
    g_PktPort = Config::PACKET_SNIFFER_TCP_PORT + g_Base;

    // Loopback da instância: base 100 -> 127.0.0.2 etc.
    int idx = g_Base / 100 + 1;
    if (idx < 1) idx = 1;
    if (idx > MAX_LOOPBACK_INDEX) {
        Logger::Log("INSTANCE", "base %d acima do limite de loopback (127.0.0.%d) — usando 127.0.0.1",
                    g_Base, MAX_LOOPBACK_INDEX);
        idx = 1;
    }
    strcpy_s(g_LocalIp, sizeof(g_LocalIp), "127.0.0.1");
    if (idx > 1) {
        g_LocalIp[8] = (char)('0' + (idx % 10));   // só o último digito muda
        g_LocalIp[9] = '\0';
    }

    GetEnvironmentVariableA("MUPROXY_SESSION", g_SessionDir, sizeof(g_SessionDir) - 1);

    if (!g_HasBase) {
        Logger::Log("INSTANCE",
                    "AVISO: MUPROXY_BASE nao chegou neste processo -> base 0. "
                    "Este main.exe pode se cruzar com o MuProxy2 da base 0.");
    }

    Logger::Log("INSTANCE", "base de portas = %d (cmd=%d pkt=%d | loopback=%s | sessao=%s)",
                g_Base, g_CmdPort, g_PktPort, g_LocalIp,
                g_SessionDir[0] ? g_SessionDir : "(padrao)");
}

int Base() { Resolve(); return g_Base; }

bool HasBase() { Resolve(); return g_HasBase; }

const char* LocalIp() { Resolve(); return g_LocalIp; }

int CommandPort() { Resolve(); return g_CmdPort; }

int PacketPort() { Resolve(); return g_PktPort; }

const char* SessionDir() {
    Resolve();
    return g_SessionDir;
}

} // namespace InstancePorts
