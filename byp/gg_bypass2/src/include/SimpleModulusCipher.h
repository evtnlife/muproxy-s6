#pragma once
#include <cstdint>

// SimpleModulus cipher - same as game's 0x998420
// Encrypts 8-byte blocks into 11-byte blocks
// Object at main.exe+0x8797778 (global, safe to read from any thread)

#define SM_BLOCK_SIZE       8   // input block size
#define SM_ENCRYPTED_SIZE   11  // output block size
#define SM_KEY_COUNT        4   // number of DWORD keys per array
#define MAX_RECV_KEYS       8   // candidatos de chave de recepção

class SimpleModulusCipher {
public:
    static bool Initialize();
    static int Encrypt(void* dest, const void* source, int size);
    static int Decrypt(void* dest, const void* source, int size);
    
    // Chama a funcao nativa SimpleModulus::Encrypt do jogo (0x998420)
    // Usa o objeto global em 0x8797778 — 100% identico ao jogo
    static int NativeEncrypt(void* dest, const void* source, int size);

    // Log verboso (hex de cada bloco). Desliga quando o módulo faz uma
    // varredura automática de pacotes (evita inundar o log).
    static void SetVerbose(bool verbose) { m_verbose = verbose; }
    static bool IsVerbose() { return m_verbose; }

    // Chave de RECEPÇÃO: o SERVIDOR cifra os pacotes C3 com uma chave
    // diferente da de envio do cliente (o objeto Obj2 do cliente, logo após o
    // Obj1). Sem isso, os C3 do servidor (character info, inventário) não
    // decifram. Retorna false se não houver um segundo objeto na memória.
    static bool InitializeReceiveKeys();

    // Decifra com a chave de RECEPÇÃO (servidor → cliente). -1 = falhou.
    static int DecryptReceive(void* dest, const void* source, int size);

private:
    static uint32_t m_keys[SM_KEY_COUNT * 4]; // Modulus, EncKey, DecKey, XORKey
    static uint32_t m_recvKeys[MAX_RECV_KEYS][SM_KEY_COUNT * 4];
    static int m_recvKeyCount;
    static bool m_recvReady;
    static bool m_initialized;
    static bool m_verbose;
    static uint32_t m_dwPrevState;  // persistent chaining state (carries across encryptions, matches game)

    static void EncryptBlock(void* dest, const void* source, int size);
    static int DecryptBlockWith(const uint32_t* keys, void* dest, const void* source);
    static int DecryptBlock(void* dest, const void* source);  // returns valid byte count (1-8) or -1 on checksum failure
    static int AddBits(void* buffer, int bufferBits, const void* bits, int startBit, int numBits);
    static int ReadBits(const void* buffer, int startBit, int numBits);
    static DWORD ModInverse32(DWORD a, DWORD m);  // per-DWORD modular inverse
};
