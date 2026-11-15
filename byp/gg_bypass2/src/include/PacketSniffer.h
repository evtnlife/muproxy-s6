#pragma once
#include <cstdint>

namespace PacketSniffer {

bool Install();
void InstallHooksForAllModules();
void Uninstall();

// Injeta pacote direto no socket do game server.
// Recebe a forma ja processada pela Camada 2 (XorData) nos pacotes C1 0x11
// (ex.: a forma que o byp loga / o usuario copia do log) e aplica so Layer-1
// (XOR+sub) antes de enviar via send(). Para o plaintext REAL de um ataque
// C1 0x11, use InjectC1Attack (que aplica XorData + Layer-1).
bool InjectToServer(const uint8_t* data, int len);

// Injeta o PLAINTEXT REAL de um ataque C1 0x11 (normal) aplicando as DUAS
// camadas:
//   plaintext -> XorData[3,len) (Camada 2) -> Layer-1 (Camada 1) -> wire
// VALIDADO (RE): C1 07 11 00 1A 78 04 -> FE 38 2E C0 C2 8B 48
bool InjectC1Attack(const uint8_t* plaintext, int len);

// Injeta o PLAINTEXT REAL de um skill attack C1 0x19 (7 bytes) com o
// pipeline completo da skill (diferente do 0x11, que vai C1 puro):
//   XorData[3,7) -> SM input [serial 0x08793700 + payload] -> SimpleModulus
//   (6 -> 11 bytes) -> C3 0D [11] -> Layer-1 -> wire
bool InjectSkillC1(const uint8_t* plaintext, int len);

} // namespace PacketSniffer
