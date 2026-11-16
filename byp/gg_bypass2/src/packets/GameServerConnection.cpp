// packets/GameServerConnection.cpp — Rastreamento da conexão com o game server
//
// Extraído do WinsockHooks (Fase 6/9 do refactor reorganize-byp-modules).
#include "GameServerConnection.h"
#include "OriginalApi.h"
#include "Logger.h"

namespace GameServerConnection {

// Socket do game server do JOGO (conectado ao MuProxy2 → relay → GS real).
// Usamos este socket para injeção porque é a MESMA conexão TCP autenticada.
static SOCKET g_GameServerSocket = INVALID_SOCKET;

// Porta do game server detectada automaticamente via pacote C116
// (0xC1 0x16 = "ConnectToServer": login server informa IP+porta do GS).
// 0 = ainda nao detectado (usa o range hardcoded abaixo).
static int g_DetectedGsPort = 0;

void SetSocket(SOCKET s)
{
    g_GameServerSocket = s;
}

SOCKET GetSocket()
{
    return g_GameServerSocket;
}

// Portas do game server (apenas trafego nelas sera decifrado)
bool IsGameServerPort(int port) {
    if (port == 55902 || (port >= 55901 && port <= 55950)) return true;
    if (g_DetectedGsPort != 0 && port == g_DetectedGsPort) return true;
    return false;
}

// ============================================================================
// Detecção da porta do GS via C116 (ConnectToServer).
// O login server NAO cifra o trafego (em claro). Formato real (22 bytes):
//   C1 16 [charID 2B LE] [IP 16B null-padded] [porta 2B LE]
// Exemplo capturado: C1 16 F4 03 "145.239.149.84\0.." 4D B3  -> porta 45901.
// Escaneamos o buffer RECV pela assinatura em claro C1 16, validamos que o
// campo IP comeca com digito e lemos a porta no offset 20 (little-endian).
// ============================================================================
void DetectGsPortFromC116(const char* data, int len)
{
    const int C116_LEN = 22;
    if (len < C116_LEN) return;
    for (int i = 0; i + C116_LEN <= len; ++i)
    {
        // Assinatura em claro de C1 16 (login server nao cifra)
        if ((unsigned char)data[i] != 0xC1 || (unsigned char)data[i + 1] != 0x16) continue;

        // Campo IP (offset 4) deve comecar com digito (primeiro char de um IPv4)
        if (data[i + 4] < '0' || data[i + 4] > '9') continue;

        // Porta no offset 20 (2B header + 2B charID + 16B IP buffer), little-endian
        int port = ((unsigned char)data[i + 21] << 8) | (unsigned char)data[i + 20];
        if (port < 1024 || port > 65535) continue;

        if (g_DetectedGsPort != port)
        {
            g_DetectedGsPort = port;
            Logger::Log("PACKETSNIFFER", "C116: porta do GS detectada = %d", port);
        }
        return;
    }
}

// Armazena a porta remota por socket para filtrar trafego
bool GetSocketPort(SOCKET s, int* outPort) {
    sockaddr_in addr;
    int addrLen = sizeof(addr);
    if (getpeername(s, (sockaddr*)&addr, &addrLen) == 0) {
        *outPort = ntohs(addr.sin_port);
        return true;
    }
    return false;
}

bool IsGameServerSocket(SOCKET s)
{
    int port = 0;
    return GetSocketPort(s, &port) && IsGameServerPort(port);
}

// ============================================================================
// SendToGameServer — envia bytes JÁ CIFRADOS no socket do game server.
// É o transporte do PacketInjector. O socket do jogo está conectado ao
// MuProxy2, que faz relay puro para o GS real.
// ============================================================================
bool SendToGameServer(const uint8_t* data, int len)
{
    if (!data || len <= 0) return false;
    if (g_GameServerSocket == INVALID_SOCKET) {
        Logger::Log("PACKETSNIFFER", "InjectToServer: GS socket nao rastreado");
        return false;
    }

    // Verifica se o socket ainda esta vivo (nao foi fechado pelo jogo)
    int optval = 0;
    int optlen = sizeof(optval);
    if (getsockopt(g_GameServerSocket, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == SOCKET_ERROR || optval != 0) {
        Logger::Log("PACKETSNIFFER", "InjectToServer: socket invalido (err=%d), limpando", optval);
        g_GameServerSocket = INVALID_SOCKET;
        return false;
    }

    // Usa o send ORIGINAL (fora do hook) para não reaparecer no log de pacotes.
    OriginalApi::SendFn origSend = OriginalApi::GetSend();
    int sent = origSend ? origSend(g_GameServerSocket, (const char*)data, len, 0)
                         : send(g_GameServerSocket, (const char*)data, len, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        Logger::Log("PACKETSNIFFER", "InjectToServer: send error %d - socket morto, limpando", err);
        g_GameServerSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

} // namespace GameServerConnection
