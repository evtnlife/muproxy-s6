#pragma once
#include "sdk.h"

// ============================================================================
// PacketChannel — Canal de PACOTES com o MuProxy2 (TCP 44413)
//
// Envia o log de pacotes capturados ("PKT|<dir>|<hex>", um pacote por linha)
// e recebe o comando "SEND <hex>" (injeção direta no game server).
//
// Não confundir com o CommandChannel (44412), que trafega os comandos do
// MuProxy2 (CMD/ENTITY).
//
// O canal usa o connect/send ORIGINAIS do ws2_32 (fora do hook), para não
// gerar log a cada tentativa de conexão.
// ============================================================================

namespace PacketChannel {

// Inicializa o canal (socket + threads de connect/recv).
bool Initialize();

// Encerra o canal (fecha o socket e para as threads).
void Shutdown();

// Envia uma linha pronta ("PKT|...\n") para o MuProxy2.
bool SendLine(const char* line, int len);

// True se o socket for o do canal de pacotes (o hook de log deve ignorá-lo).
bool IsChannelSocket(SOCKET s);

} // namespace PacketChannel
