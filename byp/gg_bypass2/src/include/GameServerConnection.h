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

// ============================================================================
// Peer REAL (destino original antes do redirecionamento para 127.0.0.1).
//
// O cliente valida LOCALMENTE o peer do socket: em Main.dll+0xBB170 ele chama
// getpeername e classifica o socket pela porta do peer (faixa 45901..55950),
// e o chamador (Main.dll+0xBB300) usa o resultado para cifrar o pacote do GS.
//
// Com o proxy, o peer viraria 127.0.0.1:<porta local> — e a porta local so
// coincide com a real na instancia base 0 (45901). Nas outras instancias a
// validacao local falha e o cliente trava na tela de selecao de sala.
//
// Guardamos o destino original no hook de connect para devolver ao cliente o
// peer verdadeiro (o mesmo que ele veria sem o MuProxy2).
// ============================================================================
void RememberRealPeer(SOCKET s, const char* host, int port);
bool GetRealPeer(SOCKET s, char* host, int hostLen, int* port);

// ============================================================================
// Detecção por DADOS (o hook de connect redireciona o jogo para 127.0.0.1,
// então getpeername devolve a porta do MuProxy2 e a heurística por porta
// não identifica o socket do GS).
//
// Marca o socket como GS se o tráfego decifrado parece um pacote MU.
bool MarkIfGameServerSocket(SOCKET s, const uint8_t* data, int len);

// Marca o socket como GS (sem inspecionar os dados).
void MarkGameServerSocket(SOCKET s);

// Socket onde o jogo está JOGANDO (detectado pelo tráfego de objetos/life).
// É o alvo da injeção: o servidor espera o pacote na MESMA conexão do jogo.
void SetGameplaySocket(SOCKET s);
SOCKET GetGameplaySocket();

// Detecta a porta do GS pelo pacote C116 (login server → cliente, em claro).
void DetectGsPortFromC116(const char* data, int len);

// Envia bytes JÁ CIFRADOS no socket do game server.
// É o transporte do PacketInjector.
bool SendToGameServer(const uint8_t* data, int len);

} // namespace GameServerConnection
