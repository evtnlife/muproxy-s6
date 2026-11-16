#pragma once
#include "sdk.h"

// ============================================================================
// OriginalApi — Ponteiros das APIs originais do ws2_32 (trampolines dos hooks)
//
// Os módulos internos que abrem socket para o MuProxy2 (CommandChannel 44412,
// PacketChannel 44413) precisam chamar o connect/send ORIGINAIS: passando pelo
// hook, o connect seria logado a cada tentativa (spam) e o endereço seria
// reescrito pelo hook de redirecionamento.
//
// O módulo de hooks (WinsockHooks) registra os trampolines aqui durante o
// Install(). Antes disso os ponteiros são nullptr — nesse caso o chamador
// deve usar a API do sistema.
// ============================================================================

namespace OriginalApi {

typedef int (WINAPI* SendFn)(SOCKET s, const char* buf, int len, int flags);
typedef int (WINAPI* ConnectFn)(SOCKET s, const sockaddr* name, int namelen);

// Registra os trampolines dos hooks (chamado pelo módulo de hooks)
void Set(SendFn send, ConnectFn connect);

// Trampolines registrados (nullptr antes do Install dos hooks)
SendFn GetSend();
ConnectFn GetConnect();

} // namespace OriginalApi
