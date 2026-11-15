#pragma once
#include <cstdint>

// ============================================================================
// PacketHandler — Dispatcher de pacotes MU Online recebidos do MuProxy2
//
// Cada pacote recebido via TCP é parseado e despachado para o handler
// correspondente baseado no opcode. Handlers são registrados separadamente
// por cada módulo (MonsterTracker, AttackModule, etc.)
//
// Formatos TCP suportados:
//
//   "CMD <opcode> <subCmd> <raw_text>"
//     → opcode e subCmd são palavras separadas por espaço
//     → raw_text é passado como bytes ASCII diretamente ao handler
//     → Ex: "CMD ATTACK START 012A delay=500 range=3.5 skill=1E type=1"
//       → opcode="ATTACK", subCmd="START", data="012A delay=500 range=3.5 skill=1E type=1"
//
//   "SEND <hex_data>"
//     → Injeção direta de pacote (bytes hexadecimais)
//     → Ex: "SEND C1 05 D4 2A 01"
// ============================================================================

namespace PacketHandler {

// Callback para handler de pacote.
// opcode: comando/módulo (ex: "ATTACK", "MONSTER")
// subCmd: sub-comando (ex: "START", "STOP", "LIST")
// data: dados brutos após opcode+subCmd (pode conter hex + key=value ASCII)
// dataLen: tamanho dos dados em bytes
typedef void (*PacketCallback)(const char* opcode, const char* subCmd,
                               const uint8_t* data, int dataLen);

// Inicializa o parser de comandos
bool Initialize();

// Processa uma linha recebida do MuProxy2
// Formatos: "CMD <opcode> <subCmd> [data]" ou "SEND <hex>"
void ProcessLine(const char* line, int len);

// Registra callback para um opcode específico
void RegisterHandler(const char* opcode, PacketCallback callback);

// Desliga o handler
void Shutdown();

} // namespace PacketHandler