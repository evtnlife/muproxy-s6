#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// GameServerConnection — Rastreamento da conexão com o game server
//
// O socket do GS é o socket do JOGO (o mesmo que o jogo usa para falar com o
// MuProxy2, que faz relay puro para o GS real). Ele é capturado no connect
// (ou no primeiro send) e é usado para a injeção de pacotes.
//
// A porta do GS é detectada pelo pacote C1 16 (ConnectToServer) que o login
// server envia em claro — o login server não cifra o tráfego.
// ============================================================================

namespace GameServerConnection {

// Marca o socket como sendo do game server (chamado pelos hooks).
void SetSocket(SOCKET s);

// Socket do game server atual (INVALID_SOCKET se ainda não capturado).
SOCKET GetSocket();

// Porta remota do socket (false se getpeername falhar).
bool GetSocketPort(SOCKET s, int* outPort);

// True se a porta for do game server (range conhecido ou porta detectada no C116).
bool IsGameServerPort(int port);

// True se o socket for do game server.
bool IsGameServerSocket(SOCKET s);

// Detecta a porta do GS pelo pacote C116 (login server → cliente, em claro).
void DetectGsPortFromC116(const char* data, int len);

// Envia bytes JÁ CIFRADOS no socket do game server.
// É o transporte do PacketInjector.
bool SendToGameServer(const uint8_t* data, int len);

} // namespace GameServerConnection
