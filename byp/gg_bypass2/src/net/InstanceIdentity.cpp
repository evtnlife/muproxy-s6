// net/InstanceIdentity.cpp — Handshake de identidade da instância (HELLO)
#include "sdk.h"
#include <cstdlib>
#include <cstring>

#include "InstanceIdentity.h"
#include "CommandChannel.h"
#include "InstancePorts.h"
#include "MessageRegistry.h"
#include "Logger.h"

namespace InstanceIdentity {

// O MuProxy2 recusou este byp? (HELLO_ACK ok=0)
static volatile LONG g_Refused = 0;
static volatile LONG g_LoggedRefusal = 0;

// ---------------------------------------------------------------------------
// HELLO — identidade do byp para o MuProxy2.
// Enviado logo depois de conectar no canal de comandos (e a cada reconexão).
// ---------------------------------------------------------------------------
static void SendHello()
{
    CommandChannel::SendFormat("HELLO|pid=%u|base=%d",
                                (unsigned)GetCurrentProcessId(), InstancePorts::Base());
}

// ---------------------------------------------------------------------------
// HELLO_ACK — resposta do MuProxy2.
//   ok=1 -> aceito (este byp pertence a esta janela do MuProxy2)
//   ok=0 -> recusado (o byp está na base errada)
// ---------------------------------------------------------------------------
static void HandleHelloAck(const char* payload)
{
    int ok = 0;
    int base = -1;

    const char* p = payload;
    while (p && *p) {
        const char* bar = strchr(p, '|');
        size_t len = bar ? (size_t)(bar - p) : strlen(p);
        if (len > 3 && strncmp(p, "ok=", 3) == 0) {
            ok = atoi(p + 3);
        } else if (len > 5 && strncmp(p, "base=", 5) == 0) {
            base = atoi(p + 5);
        }
        if (!bar) break;
        p = bar + 1;
    }

    if (ok == 0) {
        InterlockedExchange(&g_Refused, 1);
        // Log com throttle: o canal reconecta a cada segundo e o MuProxy2
        // responde ok=0 toda vez.
        static DWORD lastLog = 0;
        DWORD now = GetTickCount();
        if (InterlockedCompareExchange(&g_LoggedRefusal, 1, 0) == 0 || now - lastLog > 30000) {
            lastLog = now;
            Logger::Log("INSTANCE",
                        "O MuProxy2 recusou este byp (base desta=%d, base do MuProxy=%d) — "
                        "este main.exe NAO pertence a esta janela do MuProxy2. "
                        "As conexoes do jogo ficam bloqueadas.",
                        InstancePorts::Base(), base);
        }
    }
}
REGISTER_MESSAGE(HELLO_ACK, HandleHelloAck);

void Initialize()
{
    CommandChannel::SetOnConnectedHandler(SendHello);
    Logger::Log("INSTANCE", "identidade: pid=%u base=%d (MUPROXY_BASE %s)",
                (unsigned)GetCurrentProcessId(), InstancePorts::Base(),
                InstancePorts::HasBase() ? "ok" : "AUSENTE");
}

bool Refused()
{
    return InterlockedCompareExchange(&g_Refused, 0, 0) == 1;
}

} // namespace InstanceIdentity
