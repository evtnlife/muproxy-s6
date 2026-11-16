#include "sdk.h"
#include "MuCipher.h"
#include "Logger.h"
#include "GameMemory/Globals.h"

namespace MuCipher {

static HMODULE GetMainDllModule()
{
    return (HMODULE)GameMemory::Globals::MainDllBase();
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
// Camada 2 INVERSA (decodificacao).
//
// A conta e a mesma, mas na ordem DECRESCENTE: assim o byte anterior usado
// ainda e o CIFRADO (o Camada-2 nao e auto-inversa in-place). E exatamente o
// que o servidor faz ao receber (PacketManager::XorData(size-1, end)).
//
// Exemplo (pacote de ataque 0x11):
//   C1 07 11 EF ED B6 77  (forma Camada-2)  --UnXorData[3,7)-->  C1 07 11 00 1A 78 04
// ============================================================================
void UnXorData(char* data, int start, int end)
{
    if (start >= end) return;
    for (int n = end - 1; n >= start; --n)
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
// Para o plaintext real, decifre Layer-1 e depois aplique UnXorData em [3, len)
// (o Camada-2 NAO e auto-inverso in-place: a ordem tem que ser decrescente).
// O Camada-2 e aplicado a TODO pacote C1 do cliente para o servidor (o
// SocketManager do servidor decodifica [3,size) em ordem decrescente).
// ============================================================================
void FullDecrypt(char* data, int len, BYTE xorKey, BYTE subKey)
{
    DecryptLayer1(data, len, xorKey, subKey);
}

// ============================================================================
// Cifragem completa: APENAS Camada 1 (XOR+sub)
// ============================================================================
void FullEncrypt(char* data, int len, BYTE xorKey, BYTE subKey)
{
    EncryptLayer1(data, len, xorKey, subKey);
}

// ============================================================================
// Detecção por DADOS: o buffer criptografado começa com um pacote MU válido?
// ============================================================================
int PeekDecodedPacketSize(const BYTE* enc, int len)
{
    if (!enc || len < 3) return 0;

    unsigned char xorKey = 0, subKey = 0;
    GetGameServerKeys(&xorKey, &subKey);

    // Decifra só o começo (basta o header + o size).
    unsigned char d[4] = { 0 };
    const int n = (len < 4) ? len : 4;
    for (int i = 0; i < n; i++)
        d[i] = (unsigned char)((enc[i] ^ xorKey) - subKey);

    const int hdr = GetMuHeaderSize(d[0]);
    if (hdr <= 0) return 0;

    int size = 0;
    if (hdr == 2) {
        if (n < 2) return 0;
        size = d[1];
    } else {
        if (n < 3) return 0;
        size = (d[1] << 8) | d[2];
    }

    // Size mínimo = header + opcode; máximo = o limite de um pacote MU.
    // (O size pode ser MAIOR que o que já chegou: o pacote pode estar
    //  partido entre duas leituras do socket — ainda assim é um pacote MU.)
    if (size < hdr + 1) return 0;
    if (size > 4096) return 0;
    return size;
}

} // namespace MuCipher
