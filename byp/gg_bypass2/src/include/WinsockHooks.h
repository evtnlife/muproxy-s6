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

// O rastreamento do socket do game server (e o transporte da injeção) vive no
// módulo GameServerConnection.

} // namespace WinsockHooks
