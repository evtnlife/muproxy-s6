#pragma once
#include "sdk.h"

// ============================================================================
// CommandChannel — Canal de COMANDOS com o MuProxy2 (TCP 44412)
//
// Envia e recebe linhas de texto via TCP para 127.0.0.1:44412.
// Thread-safe via CRITICAL_SECTION. Reconexão automática em thread separada.
//
// Protocolo: cada mensagem é uma linha terminada em \n.
// Prefixos: "CMD " para comandos, "ENTITY" para a lista de monstros.
//
// Não confundir com o PacketChannel (44413), que trafega o log de pacotes.
// ============================================================================

namespace CommandChannel {

bool Initialize(const char* host = "127.0.0.1", int port = 44412);
void Shutdown();

// Envia dados brutos pelo socket. Thread-safe.
bool Send(const char* data, int len);

// Envia uma linha completa (adiciona \n se não tiver). Thread-safe.
bool SendLine(const char* line, int len);

// Envia linha formatada. Thread-safe.
bool SendFormat(const char* fmt, ...);

// Retorna o socket atual (para uso em recv thread externa)
SOCKET GetSocket();

// True se o socket for o do canal de comandos (o hook de log deve ignorá-lo).
bool IsChannelSocket(SOCKET s);

// Verifica se está conectado
bool IsConnected();

// Fecha e reconecta
void Reconnect();

} // namespace CommandChannel