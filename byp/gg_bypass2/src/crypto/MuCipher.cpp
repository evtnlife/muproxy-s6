#include "sdk.h"
#include "MuCipher.h"
#include "Logger.h"
#include "GameMemory.h"

namespace MuCipher {

static HMODULE GetMainDllModule()
{
    return (HMODULE)GameMemory::GetMainDllBase();
}

bool GetGameServerKeys(unsigned char* xorKey, unsigned char* subKey)
{
    // Chaves hardcoded - confirmadas via x32dbg em Main.dll+0x403F75/76
    *xorKey = 0x36;
    *subKey = 0x07;

    // Loga apenas uma vez
    static bool logged = false;
    if (!logged) {
        logged = true;
        HMODULE hMain = GetMainDllModule();
        if (hMain) {
            uintptr_t base = (uintptr_t)hMain;
            unsigned char memSub = *(unsigned char*)(base + 0x403F75);
            unsigned char memXor = *(unsigned char*)(base + 0x403F76);
            Logger::Log("MUCIPHER", "Chaves: xor=0x%02X sub=0x%02X (mem: xor=0x%02X sub=0x%02X)",
                *xorKey, *subKey, memXor, memSub);
        } else {
            Logger::Log("MUCIPHER", "Chaves: xor=0x%02X sub=0x%02X", *xorKey, *subKey);
        }
    }
    return true;
}

// ============================================================================
// XorFilter — Camada 2 de criptografia (PacketManager::XorData)
// Tabela REAL lida do binario Main.dll em 0x7166FF20 (via x32dbg)
// ============================================================================
static const BYTE g_XorFilter[32] = {
    0xAB, 0x11, 0xCD, 0xFE, 0x18, 0x23, 0xC5, 0xA3,
    0xCA, 0x33, 0xC1, 0xCC, 0x66, 0x67, 0x21, 0xF3,
    0x32, 0x12, 0x15, 0x35, 0x29, 0xFF, 0xFE, 0x1D,
    0x44, 0xEF, 0xCD, 0x41, 0x26, 0x3C, 0x4E, 0x4D
};

int GetMuHeaderSize(BYTE firstByte)
{
    switch (firstByte) {
        case 0xC1: case 0xC3: return 2;
        case 0xC2: case 0xC4: return 3;
        default: return 0;
    }
}

// ============================================================================
// Camada 1: XOR+sub byte-a-byte (HackCheck::DecryptData)
// ============================================================================
void DecryptLayer1(char* data, int len, BYTE xorKey, BYTE subKey)
{
    for (int i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)data[i];
        data[i] = (char)((c ^ xorKey) - subKey);
    }
}

// ============================================================================
// Camada 1: XOR+sub byte-a-byte (HackCheck::EncryptData)
// ============================================================================
void EncryptLayer1(char* data, int len, BYTE xorKey, BYTE subKey)
{
    for (int i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)data[i];
        data[i] = (char)(((c + subKey) ^ xorKey));
    }
}

// ============================================================================
// Camada 2: XOR encadeado no corpo do pacote (PacketManager::XorData)
// ============================================================================
void XorData(char* data, int start, int end)
{
    if (start >= end) return;
    for (int n = start; n < end; ++n)
    {
        data[n] ^= (data[n - 1] ^ g_XorFilter[n % 32]);
    }
}

// ============================================================================
// Decifracao: Camada 1 (XOR+sub) — usada para display no log.
//
// ATENCAO (RE validada, 4 capturas): o BlackRock USa XorFilter (Camada 2) nos
// pacotes de ataque C1 0x11. A cadeia real e:
//   WIRE = Layer1( XorData[3,7)( plaintext ) )
// Decifrar SO com Layer-1 (esta funcao) devolve a forma XorData
// (ex.: "C1 07 11 EF ED B6 77"), NAO o plaintext real ("C1 07 11 00 1A 78 04").
// Para o plaintext real, decifre Layer-1 e depois aplique XorData
// (que e sua propria inversa) em [3, len).
// O uso de XorData em outros opcodes C1 (ex.: movimento D4) ainda nao confirmado.
// ============================================================================
void FullDecrypt(char* data, int len, BYTE xorKey, BYTE subKey)
{
    DecryptLayer1(data, len, xorKey, subKey);
}

// ============================================================================
// Cifracao: APENAS Camada 1 (XOR+sub)
// ============================================================================
void FullEncrypt(char* data, int len, BYTE xorKey, BYTE subKey)
{
    EncryptLayer1(data, len, xorKey, subKey);
}

} // namespace MuCipher
