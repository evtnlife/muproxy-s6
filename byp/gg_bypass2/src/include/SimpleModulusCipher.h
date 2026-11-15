#pragma once
#include <cstdint>

// SimpleModulus cipher - same as game's 0x998420
// Encrypts 8-byte blocks into 11-byte blocks
// Object at main.exe+0x8797778 (global, safe to read from any thread)

#define SM_BLOCK_SIZE       8   // input block size
#define SM_ENCRYPTED_SIZE   11  // output block size
#define SM_KEY_COUNT        4   // number of DWORD keys per array

class SimpleModulusCipher {
public:
    static bool Initialize();
    static int Encrypt(void* dest, const void* source, int size);
    static int Decrypt(void* dest, const void* source, int size);
    
    // Chama a funcao nativa SimpleModulus::Encrypt do jogo (0x998420)
    // Usa o objeto global em 0x8797778 — 100% identico ao jogo
    static int NativeEncrypt(void* dest, const void* source, int size);
    
    // Chama a funcao nativa com EncKey/DecKey trocados para decryptar
    // Cria copia local do objeto, swap EncKey<->DecKey, chama NativeEncrypt
    static int NativeDecrypt(void* dest, const void* source, int size);
    
private:
    static uint32_t m_keys[SM_KEY_COUNT * 4]; // Modulus, EncKey, DecKey, XORKey
    static bool m_initialized;
    
    static void EncryptBlock(void* dest, const void* source, int size);
    static int DecryptBlock(void* dest, const void* source);  // returns valid byte count (1-8) or -1 on checksum failure
    static int AddBits(void* buffer, int bufferBits, const void* bits, int startBit, int numBits);
    static int ReadBits(const void* buffer, int startBit, int numBits);
    static DWORD ModInverse32(DWORD a, DWORD m);  // per-DWORD modular inverse
};
