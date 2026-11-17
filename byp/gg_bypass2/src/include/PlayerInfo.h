#pragma once
// ============================================================================
// PlayerInfo — envia periodicamente (1x/s) o estado do personagem local para
// o MuProxy2 via canal de comandos:
//
//   PLAYER_INFO|name=<nome>|level=<n>|map=<n>
//
// O MuProxy2 usa isso para mostrar nome/level/mapa no header da janela.
// O x,y NÃO vai por aqui — o proxy já rastreia a posição via pacotes D4.
// ============================================================================

namespace PlayerInfo {

void Initialize();
void Shutdown();

} // namespace PlayerInfo
