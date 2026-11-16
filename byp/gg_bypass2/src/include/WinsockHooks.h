#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// WinsockHooks — Hooks inline nas exports do ws2_32.dll
//
// módulo dono de:
//   - trampolines (InstallInlineHook / AllocateTrampoline)
//   - hooks: send, recv, WSASend, WSARecv, sendto, recvfrom, connect, WSAConnect
//   - completion routine do WSARecv overlapped
//   - rastreamento do socket do game server (usado na injeção)
//
// RESTRIÇÃO: Install() é chamado SINCRONAMENTE no DLL_PROCESS_ATTACH, antes
// de qualquer thread — é o que permite capturar o connect do jogo ao game
// server. Não mover essa chamada para uma thread.
//
// Os pacotes capturados são entregues ao observador (PacketSniffer), que faz
// a decifração, o reassembly e o log.
// ============================================================================

namespace WinsockHooks {

// Observador de pacotes capturados.
// dir = "SEND" | "RECV"; data = bytes crus (criptografados), len = tamanho.
typedef void (*PacketObserver)(const char* dir, SOCKET s, const char* data, int len);

// Notificação de socket encerrado (o observador descarta o buffer de reassembly).
typedef void (*SocketClosedObserver)(SOCKET s);

// Registra os observadores (o PacketSniffer monta o log/reassembly).
void SetPacketObserver(PacketObserver observer);
void SetSocketClosedObserver(SocketClosedObserver observer);

// Instala os hooks. Chamado de forma síncrona no DLL_PROCESS_ATTACH.
bool Install();

// Remove os hooks (restaura os bytes originais).
void Uninstall();

// ---- Rastreamento do game server ----

// True se o socket pertence ao game server (porta conhecida ou detectada no C116).
bool IsGameServerSocket(SOCKET s);

// Detecta a porta do GS pelo pacote C116 (login server → cliente, em claro).
void DetectGsPortFromC116(const char* data, int len);

// Envia bytes JÁ CIFRADOS no socket do game server.
// É o transporte do PacketInjector. O socket do jogo está conectado ao
// MuProxy2, que faz relay puro para o GS real.
bool SendToGameServer(const uint8_t* data, int len);

} // namespace WinsockHooks
