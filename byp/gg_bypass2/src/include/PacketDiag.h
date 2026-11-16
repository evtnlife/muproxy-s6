#pragma once
#include "sdk.h"
#include <cstdint>

// ============================================================================
// PacketDiag — diagnóstico do tráfego S→C (servidor → cliente)
//
// Motivo: o log principal (gg_bypass2.log) é APPEND e vive na pasta do jogo —
// o usuário às vezes apaga/renomeia ele e perde justamente os pacotes de
// login (character info, inventário), que são os que o auto-potion precisa.
//
// Este módulo grava num arquivo PRÓPRIO (gg_bypass2_s2c.log), TRUNCADO no
// attach da DLL, com:
//   - os primeiros N pacotes S→C em hex (header, opcode, tamanho)
//   - o histograma de opcodes S→C (qual opcode o servidor realmente usa)
//
// O histograma é a informação decisiva: com ele dá para ver, por exemplo,
// se o update de life vem em 0x26 mesmo ou em outro opcode.
// ============================================================================

namespace PacketDiag {

// Abre o arquivo de diagnóstico (trunca se já existir). dirPath = pasta do jogo.
bool Initialize(const char* dirPath);

// Grava um pacote decifrado (header + opcode + hex). dir = "RECV"/"SEND".
void LogPacket(const char* dir, const uint8_t* data, int len);

// Grava uma linha de texto no arquivo de diagnóstico.
void LogLine(const char* fmt, ...);

// Grava um buffer em hex com uma etiqueta (ex.: corpo decodificado de um C3).
void LogHex(const char* tag, const uint8_t* data, int len);

// Grava o histograma de opcodes (no arquivo de diagnóstico e no log principal).
void LogHistogram();

// Fecha o arquivo.
void Shutdown();

} // namespace PacketDiag
