#pragma once
#include "sdk.h"

// ============================================================================
// TcpClient — Conexão TCP com o MuProxy2
//
// Envia e recebe linhas de texto via TCP para 127.0.0.1:44412.
// Thread-safe via CRITICAL_SECTION. Reconexão automática em thread separada.
//
// Protocolo: cada mensagem é uma linha terminada em \n.
// Prefixos: "PKT " para pacotes, "CMD " para comandos, "LOG " para logs.
// ============================================================================

namespace TcpClient {

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

// Verifica se está conectado
bool IsConnected();

// Fecha e reconecta
void Reconnect();

} // namespace TcpClient