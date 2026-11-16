#pragma once
#include "sdk.h"

// ============================================================================
// MuCipher — Cifra de pacotes MU Online (módulo único de criptografia)
//
// Camada 1 (HackCheck): XOR+sub byte-a-byte — EncryptLayer1/DecryptLayer1.
// Camada 2 (PacketManager::XorData): XOR encadeado no payload [start, end).
//
// XorData é a ÚNICA porta de XOR do projeto e é a própria inversa:
// aplicar duas vezes devolve o plaintext.
//
// O SM (SimpleModulusCipher) é módulo separado — aplica-se apenas ao caminho
// C3 (skill 0x19), não é "camada" genérica.
// ============================================================================

namespace MuCipher {

// Camada 1: XOR+sub byte-a-byte (HackCheck::DecryptData)
void DecryptLayer1(char* data, int len, BYTE xorKey, BYTE subKey);

// Camada 1: XOR+sub byte-a-byte (HackCheck::EncryptData)
void EncryptLayer1(char* data, int len, BYTE xorKey, BYTE subKey);

// Camada 2: XOR encadeado no corpo do pacote (PacketManager::XorData).
// Tabela g_XorFilter lida do binário Main.dll em 0x7166FF20 (via x32dbg).
void XorData(char* data, int start, int end);

// Camada 2 INVERSA (decodificação): mesma conta, ordem DECRESCENTE.
// É o que o servidor faz ao receber (PacketManager::XorData(size-1, end)).
// Necessário para ler o payload de pacotes capturados — o Camada-2 NÃO é
// auto-inversa in-place (o byte anterior usado é o CIFRADO), então a ordem
// decrescente é obrigatória.
void UnXorData(char* data, int start, int end);

// Decifração completa: APENAS Camada 1 (usada para display no log).
void FullDecrypt(char* data, int len, BYTE xorKey, BYTE subKey);

// Cifragem completa: APENAS Camada 1 (XOR+sub).
void FullEncrypt(char* data, int len, BYTE xorKey, BYTE subKey);

// Chaves do game server (hardcoded, confirmadas via x32dbg em Main.dll+0x403F75/76)
bool GetGameServerKeys(unsigned char* xorKey, unsigned char* subKey);

// Tamanho do header MU pelo byte de tipo (C1/C3=2, C2/C4=3, outro=0)
int GetMuHeaderSize(BYTE firstByte);

// ============================================================================
// Detecção por DADOS: o buffer criptografado (Camada-1) começa com um pacote
// MU válido? Devolve o tamanho total do pacote (header + payload) ou 0.
//
// Usado para descobrir QUAL socket é o do game server: o hook de connect
// redireciona o jogo para 127.0.0.1 (MuProxy2), então getpeername() devolve a
// porta do proxy e a heurística por porta não funciona. O que não falha é o
// conteúdo: um pacote MU decifrado tem header C1/C2/C3/C4 e um size coerente.
// ============================================================================
int PeekDecodedPacketSize(const BYTE* enc, int len);

} // namespace MuCipher
