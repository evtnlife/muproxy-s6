#pragma once
#include <cstdint>

namespace PacketSniffer {

bool Install();
void InstallHooksForAllModules();
void Uninstall();

// A injeção de pacotes no game server vive no módulo PacketInjector:
//   PacketInjector::Inject(plaintext, len)      → porta única (XorData + pipeline)
//   PacketInjector::InjectXorData(data, len)    → dados já na forma XorData
// Este módulo é o dono do socket do game server e registra o transporte
// (SendToGameServer) no PacketInjector durante o Install().

} // namespace PacketSniffer
