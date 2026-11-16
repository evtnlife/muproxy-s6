// packets/GameServerConnection.cpp — Rastreamento da conexão com o game server
//
// Extraído do WinsockHooks (Fase 6/9 do refactor reorganize-byp-modules).
#include "GameServerConnection.h"
#include "OriginalApi.h"
#include "MuCipher.h"
#include "Logger.h"

namespace GameServerConnection {

// Socket do game server do JOGO (conectado ao MuProxy2 → relay → GS real).
// Usamos este socket para injeção porque é a MESMA conexão TCP autenticada.
static SOCKET g_GameServerSocket = INVALID_SOCKET;

// Socket onde o jogo está JOGANDO (tráfego de objetos/life). A injeção tem
// que sair por aqui: o servidor processa o pacote na sessão da conexão.
static SOCKET g_GameplaySocket = INVALID_SOCKET;

// Sockets confirmados como GS pela ANÁLISE DO TRÁFEGO (detecção por dados).
static const int MAX_GS_SOCKETS = 16;
static SOCKET g_GsSockets[MAX_GS_SOCKETS];
static int    g_GsSocketCount = 0;

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

void SetGameplaySocket(SOCKET s)
{
    if (s == INVALID_SOCKET) return;
    if (g_GameplaySocket != s) {
        Logger::Log("PACKETSNIFFER", "socket de gameplay = %d (antes: %d)", (int)s, (int)g_GameplaySocket);
    }
    g_GameplaySocket = s;
    g_GameServerSocket = s;   // a injeção segue o gameplay
    MarkGameServerSocket(s);
}

SOCKET GetGameplaySocket()
{
    return g_GameplaySocket;
}

void MarkGameServerSocket(SOCKET s)
{
    if (s == INVALID_SOCKET) return;
    for (int i = 0; i < g_GsSocketCount; i++)
        if (g_GsSockets[i] == s) return;
    if (g_GsSocketCount < MAX_GS_SOCKETS)
        g_GsSockets[g_GsSocketCount++] = s;
}

// ============================================================================
// Detecção por DADOS: o socket é do GS se o tráfego decifrado (Camada-1)
// começa com um pacote MU válido (header C1/C2/C3/C4 + size coerente).
//
// Isso substitui a heurística por PORTA: o hook de connect redireciona o
// jogo para 127.0.0.1 (MuProxy2) e a porta real do GS (44411 no BlackRock)
// não está no range hardcoded — o tráfego nunca era decifrado e nenhum
// pacote S→C era reconstruído.
// ============================================================================
bool MarkIfGameServerSocket(SOCKET s, const uint8_t* data, int len)
{
    if (s == INVALID_SOCKET || !data || len <= 0) return false;

    for (int i = 0; i < g_GsSocketCount; i++)
        if (g_GsSockets[i] == s) return true;

    const int pktSize = MuCipher::PeekDecodedPacketSize(data, len);
    if (pktSize <= 0) return false;

    MarkGameServerSocket(s);
    Logger::Log("PACKETSNIFFER", "socket %d detectado como GS pelo trafego (pacote MU de %d bytes)",
                (int)s, pktSize);
    return true;
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
    if (s == INVALID_SOCKET) return false;

    // Sockets confirmados pela análise do tráfego (detecção por dados).
    for (int i = 0; i < g_GsSocketCount; i++)
        if (g_GsSockets[i] == s) return true;

    // Socket rastreado no connect / no gameplay.
    if (s == g_GameServerSocket) return true;

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

    // Alvo: a conexão onde o jogo está JOGANDO (o servidor processa o pacote
    // na sessão dessa conexão). Cai no socket rastreado se ainda não houver.
    SOCKET target = (g_GameplaySocket != INVALID_SOCKET) ? g_GameplaySocket : g_GameServerSocket;

    if (target == INVALID_SOCKET) {
        Logger::Log("PACKETSNIFFER", "InjectToServer: GS socket nao rastreado");
        return false;
    }

    // Verifica se o socket ainda esta vivo (nao foi fechado pelo jogo)
    int optval = 0;
    int optlen = sizeof(optval);
    if (getsockopt(target, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == SOCKET_ERROR || optval != 0) {
        Logger::Log("PACKETSNIFFER", "InjectToServer: socket invalido (err=%d), limpando", optval);
        if (target == g_GameServerSocket) g_GameServerSocket = INVALID_SOCKET;
        if (target == g_GameplaySocket) g_GameplaySocket = INVALID_SOCKET;
        return false;
    }

    // Usa o send ORIGINAL (fora do hook) para não reaparecer no log de pacotes.
    OriginalApi::SendFn origSend = OriginalApi::GetSend();
    int sent = origSend ? origSend(target, (const char*)data, len, 0)
                         : send(target, (const char*)data, len, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        Logger::Log("PACKETSNIFFER", "InjectToServer: send error %d - socket morto, limpando", err);
        if (target == g_GameServerSocket) g_GameServerSocket = INVALID_SOCKET;
        if (target == g_GameplaySocket) g_GameplaySocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

} // namespace GameServerConnection
