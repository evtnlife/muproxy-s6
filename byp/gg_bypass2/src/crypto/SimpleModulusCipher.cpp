#include "sdk.h"
#include "SimpleModulusCipher.h"
#include "GameMemory/Globals.h"
#include "Logger.h"
#include <cstring>
#include <cstdio>
#include <cstdint>

uint32_t SimpleModulusCipher::m_keys[SM_KEY_COUNT * 4] = {0};
bool SimpleModulusCipher::m_initialized = false;
static int g_initCount = 0;  // limit verbose logging to first init only

// Game's SimpleModulus object at main.exe+0x8397778 (RVA)
// Absolute: 0x08797778 (confirmed via x64dbg at 0x404E73: mov ecx, 0x8797778)
//
// SM Object layout (72 bytes total, confirmed via x64dbg disassembly):
//   +0x00: DWORD prefix     (always 0x00D546EC, not used by encrypt/decrypt)
//   +0x04: DWORD Modulus[4]  (16 bytes)
//   +0x14: DWORD Key[4]     (16 bytes) — EncKey for Obj1, DecKey for Obj2
//   +0x24: DWORD Key2[4]    (16 bytes) — DecKey for Obj1 (zeros!), EncKey for Obj2
//   +0x34: DWORD XorKey[4]  (16 bytes) — XOR keys for chain cipher
//   +0x44: DWORD flags      (0 or 1)
//
// The game's encrypt function uses Key at +0x14 and XorKey at +0x34.
// The game's decrypt function uses Key2 at +0x24 (DecKey) and XorKey at +0x34.
// For C2S packets, Obj1 at 0x08797778 has EncKey at +0x14 and DecKey=zeros at +0x24.
// Dec1 keys must be computed via 128-bit modular inverse of Enc1 keys.
#define SM_OBJECT_OFFSET    0x8397778
#define OFF_PREFIX           0x00
#define OFF_MODULUS          0x04
#define OFF_ENCKEY           0x14
#define OFF_DECKEY           0x24
#define OFF_XORKEY            0x34

bool SimpleModulusCipher::Initialize() {
    if (m_initialized) return true;
    
    uintptr_t base = GameMemory::Globals::MainBase();
    if (!base) {
        Logger::Log("SMOD", "Initialize FAILED: GetMainBase() returned 0");
        return false;
    }
    
    uintptr_t obj = base + SM_OBJECT_OFFSET;
    
    __try {
        DWORD* prefix  = (DWORD*)(obj + OFF_PREFIX);
        DWORD* modulus = (DWORD*)(obj + OFF_MODULUS);
        
        // Keys are only valid after game initializes them (post-login)
        if (modulus[0] == 0 && modulus[1] == 0 && modulus[2] == 0 && modulus[3] == 0) {
            Logger::Log("SMOD", "Initialize: modulus all zero at 0x%p — keys not ready", (void*)obj);
            return false;  // not ready yet, try again later
        }
        
        DWORD* encKey = (DWORD*)(obj + OFF_ENCKEY);
        DWORD* decKeyStored = (DWORD*)(obj + OFF_DECKEY);
        DWORD* xorKey = (DWORD*)(obj + OFF_XORKEY);
        
        // Log raw SM object bytes for verification (first init only)
        g_initCount++;
        if (g_initCount == 1) {
            Logger::Log("SMOD", "SM object at 0x%p, prefix=0x%08X, first 72 bytes:", (void*)obj, prefix[0]);
            uint8_t* raw = (uint8_t*)obj;
            for (int row = 0; row < 5; row++) {
                Logger::Log("SMOD", "  +%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                    row * 16,
                    raw[row*16+0], raw[row*16+1], raw[row*16+2], raw[row*16+3],
                    raw[row*16+4], raw[row*16+5], raw[row*16+6], raw[row*16+7],
                    raw[row*16+8], raw[row*16+9], raw[row*16+10], raw[row*16+11],
                    raw[row*16+12], raw[row*16+13], raw[row*16+14], raw[row*16+15]);
            }
        }
        
        for (int i = 0; i < SM_KEY_COUNT; i++) {
            m_keys[0*SM_KEY_COUNT + i] = modulus[i];
            m_keys[1*SM_KEY_COUNT + i] = encKey[i];
            m_keys[3*SM_KEY_COUNT + i] = xorKey[i];
        }
        
        DWORD decKey[4];
        for (int i = 0; i < SM_KEY_COUNT; i++) {
            decKey[i] = ModInverse32(encKey[i], modulus[i]);
        }
        for (int i = 0; i < SM_KEY_COUNT; i++) {
            m_keys[2*SM_KEY_COUNT + i] = decKey[i];
        }
        
        Logger::Log("SMOD", "Keys loaded (#%d):", g_initCount);
        Logger::Log("SMOD", "  Mod=[%08X %08X %08X %08X]", m_keys[0], m_keys[1], m_keys[2], m_keys[3]);
        Logger::Log("SMOD", "  Enc=[%08X %08X %08X %08X]", m_keys[4], m_keys[5], m_keys[6], m_keys[7]);
        Logger::Log("SMOD", "  Dec=[%08X %08X %08X %08X] (stored=[%08X %08X %08X %08X])",
            m_keys[8], m_keys[9], m_keys[10], m_keys[11],
            decKeyStored[0], decKeyStored[1], decKeyStored[2], decKeyStored[3]);
        Logger::Log("SMOD", "  Xor=[%08X %08X %08X %08X]", m_keys[12], m_keys[13], m_keys[14], m_keys[15]);
        
        if (g_initCount == 1) {
            Logger::Log("SMOD", "  Verify mod inverse:");
            for (int i = 0; i < SM_KEY_COUNT; i++) {
                DWORD product = (DWORD)(((uint64_t)encKey[i] * decKey[i]) % modulus[i]);
                Logger::Log("SMOD", "    [%d] (0x%08X * 0x%08X) %% 0x%08X = 0x%08X %s",
                    i, encKey[i], decKey[i], modulus[i], product, product == 1 ? "OK" : "FAIL");
            }
            
            uint8_t testData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
            uint8_t encrypted[11];
            uint8_t decrypted[8];
            
            int encLen = Encrypt(encrypted, testData, 8);
            if (encLen > 0) {
                char hex[64];
                int hp = 0;
                for (int j = 0; j < encLen && hp < (int)sizeof(hex)-4; j++)
                    hp += snprintf(hex+hp, sizeof(hex)-hp, "%02X ", encrypted[j]);
                Logger::Log("SMOD", "  Roundtrip: encrypted(%d)=[%s]", encLen, hex);
                
                int decLen = Decrypt(decrypted, encrypted, encLen);
                if (decLen > 0) {
                    char dhex[32];
                    int dp = 0;
                    for (int j = 0; j < decLen && dp < (int)sizeof(dhex)-4; j++)
                        dp += snprintf(dhex+dp, sizeof(dhex)-dp, "%02X ", decrypted[j]);
                    Logger::Log("SMOD", "  Roundtrip: decrypted(%d)=[%s] %s", decLen, dhex,
                        memcmp(testData, decrypted, 8) == 0 ? "MATCH" : "MISMATCH");
                } else {
                    Logger::Log("SMOD", "  Roundtrip: DECRYPT FAILED (decLen=%d)", decLen);
                }
            } else {
                Logger::Log("SMOD", "  Roundtrip: ENCRYPT FAILED (encLen=%d)", encLen);
            }
            
            uint8_t testNative[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
            uint8_t nativeEnc[11];
            uint8_t nativeDec[8];
            
            int nEncLen = NativeEncrypt(nativeEnc, testNative, 8);
            if (nEncLen > 0) {
                char nhex[64];
                int np = 0;
                for (int j = 0; j < nEncLen && np < (int)sizeof(nhex)-4; j++)
                    np += snprintf(nhex+np, sizeof(nhex)-np, "%02X ", nativeEnc[j]);
                Logger::Log("SMOD", "  NativeEncrypt->ourDecrypt: nativeEnc(%d)=[%s]", nEncLen, nhex);
                
                int nDecLen = Decrypt(nativeDec, nativeEnc, nEncLen);
                if (nDecLen > 0) {
                    char ndhex[32];
                    int ndp = 0;
                    for (int j = 0; j < nDecLen && ndp < (int)sizeof(ndhex)-4; j++)
                        ndp += snprintf(ndhex+ndp, sizeof(ndhex)-ndp, "%02X ", nativeDec[j]);
                    Logger::Log("SMOD", "  NativeEncrypt->ourDecrypt: decrypted(%d)=[%s] %s", nDecLen, ndhex,
                        memcmp(testNative, nativeDec, 8) == 0 ? "MATCH" : "MISMATCH");
                } else {
                    Logger::Log("SMOD", "  NativeEncrypt->ourDecrypt: DECRYPT FAILED (decLen=%d)", nDecLen);
                }
            } else {
                Logger::Log("SMOD", "  NativeEncrypt->ourDecrypt: NATIVE ENCRYPT FAILED (encLen=%d)", nEncLen);
            }
        } // end if (g_initCount == 1)
        
        m_initialized = true;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Logger::Log("SMOD", "Initialize: EXCEPTION reading memory at 0x%p", (void*)obj);
        return false;
    }
}

int SimpleModulusCipher::Encrypt(void* dest, const void* source, int size) {
    if (!m_initialized && !Initialize()) return -1;
    
    int totalSize = ((size + SM_BLOCK_SIZE - 1) / SM_BLOCK_SIZE) * SM_ENCRYPTED_SIZE;
    
    if (dest == NULL) return totalSize;
    
    BYTE* src = (BYTE*)source;
    BYTE* dst = (BYTE*)dest;
    
    for (int i = 0; i < size; i += SM_BLOCK_SIZE, src += SM_BLOCK_SIZE, dst += SM_ENCRYPTED_SIZE) {
        int blockSize = (size - i < SM_BLOCK_SIZE) ? (size - i) : SM_BLOCK_SIZE;
        EncryptBlock(dst, src, blockSize);
    }
    
    return totalSize;
}

// ============================================================================
// NativeEncrypt: chama SimpleModulus::Encrypt do jogo (0x998420)
// Usa o objeto global em 0x8797778 — 100% identico ao jogo
// ============================================================================
int SimpleModulusCipher::NativeEncrypt(void* dest, const void* source, int size) {
    uintptr_t base = GameMemory::Globals::MainBase();
    if (!base) return -1;
    
    uintptr_t smObj = base + SM_OBJECT_OFFSET;  // 0x8797778
    typedef int (__thiscall *SMEncryptFn)(void* smObj, void* dest, const void* source, int size);
    SMEncryptFn fn = (SMEncryptFn)(base + 0x598420);  // 0x998420
    
    return fn((void*)smObj, dest, source, size);
}

// NOTA: o NativeDecrypt do jogo (0x998490) foi removido — ele falha em C2S
// porque o DecKey do Obj1 (0x8797778, slot +0x24) e zero. A Decrypt own
// (inversa modular do EncKey) e o caminho unico de decifragem.

void SimpleModulusCipher::EncryptBlock(void* dest, const void* source, int size) {
    DWORD* modulus = (DWORD*)&m_keys[0 * SM_KEY_COUNT];
    DWORD* encKey  = (DWORD*)&m_keys[1 * SM_KEY_COUNT];
    DWORD* xorKey  = (DWORD*)&m_keys[3 * SM_KEY_COUNT];
    
    DWORD ring[SM_KEY_COUNT];
    memset(dest, 0, SM_ENCRYPTED_SIZE);
    
    BYTE* srcPtr = (BYTE*)source;
    DWORD dwPrev = 0;
    
    // Pass 1: encrypt each 2-byte word (matches C# Encode8To11)
    // Use uint64_t multiplication to match C# uint*uint behavior
    for (int i = 0; i < SM_KEY_COUNT; i++, srcPtr += 2) {
        DWORD dwNext = 0;
        memcpy(&dwNext, srcPtr, 2);
        
        ring[i] = (DWORD)(((uint64_t)(xorKey[i] ^ (dwNext ^ dwPrev)) * encKey[i]) % modulus[i]);
        dwPrev = ring[i] & 0xFFFF;
    }
    
    // Pass 2: XOR chain backwards (matches C# Encode8To11)
    DWORD r0 = ring[0], r1 = ring[1], r2 = ring[2], r3 = ring[3];
    ring[2] ^= xorKey[2] ^ (r3 & 0xFFFF);
    ring[1] ^= xorKey[1] ^ (r2 & 0xFFFF);
    ring[0] ^= xorKey[0] ^ (r1 & 0xFFFF);
    
    // Pack bits using ring byte buffers — matches C# ShiftBytes + BitConverter.GetBytes
    // Each ring value: 16 bits from offset 0x00, then 2 bits from offset 0x10
    // Pack ring values into output stream using MSB-first bit packing
    // Each ring DWORD is stored as: 16 base bits at stream offset 0, then 2 overflow bits at stream offset 0x16=22
    // This matches C# ShiftBytes which uses offset 0x16 for the overflow bits
    int totalBits = 0;
    for (int i = 0; i < SM_KEY_COUNT; i++) {
        BYTE ringBuf[4];
        memcpy(ringBuf, &ring[i], 4);  // little-endian byte representation
        totalBits = AddBits(dest, totalBits, ringBuf, 0x00, 0x10);   // 16 base bits
        totalBits = AddBits(dest, totalBits, ringBuf, 0x16, 0x02);   // 2 overflow bits at stream offset 0x16=22
    }
    
    // Checksum: matches C# Encode8To11
    BYTE finale[2];
    finale[1] = 0xF8;
    for (int k = 0; k < SM_BLOCK_SIZE; k++) {
        BYTE pb = (k < size) ? ((BYTE*)source)[k] : 0;
        finale[1] ^= pb;
    }
    finale[0] = ((BYTE)size ^ 0x3D) ^ finale[1];
    totalBits = AddBits(dest, totalBits, finale, 0, 16);
}

// ============================================================================
// AddBits / ReadBits — MSB-first bit packing (matches game's ShiftBytes)
//
// The game's SimpleModulus uses MSB-first bit ordering:
//   Bit offset 0 in stream = bit 7 of byte[0] (most significant bit)
//   Bit offset 7 = bit 0 of byte[0] (least significant bit)
//   Bit offset 8 = bit 7 of byte[1]
//   Formula: byte = offset / 8, bit_within_byte = 7 - (offset % 8)
//
// When writing a DWORD value, bits are packed MSB-first into the stream.
// Stream bit (startBit + i) = value bit (numBits - 1 - i)
//   i.e., the most significant bit of the value comes first in the stream.
// ============================================================================

int SimpleModulusCipher::AddBits(void* buffer, int bufferBits, const void* bits, int startBit, int numBits) {
    if (numBits == 0) return bufferBits;
    
    BYTE* buf = (BYTE*)buffer;
    const BYTE* src = (const BYTE*)bits;
    
    for (int i = 0; i < numBits; i++) {
        // Read source bit at stream position (startBit + i) using MSB-first
        int srcTotalBit = startBit + i;
        int srcByte = srcTotalBit / 8;
        int srcBitOff = 7 - (srcTotalBit % 8);  // MSB-first: stream bit 0 = bit 7 of byte
        int bit = (src[srcByte] >> srcBitOff) & 1;
        
        // Write to destination at stream position (bufferBits + i) using MSB-first
        int dstTotalBit = bufferBits + i;
        int dstByte = dstTotalBit / 8;
        int dstBitOff = 7 - (dstTotalBit % 8);  // MSB-first
        
        if (bit)
            buf[dstByte] |= (1 << dstBitOff);
        else
            buf[dstByte] &= ~(1 << dstBitOff);
    }
    
    return bufferBits + numBits;
}

// ReadBits: reads numBits from buffer starting at startBit (MSB-first stream)
// and reconstructs a DWORD value matching ShiftBytes + BitConverter.ToUInt32 behavior.
// Within each byte group of 8 stream bits, the MSB-first ordering maps to
// byte[value_byte_index] bit positions that, when interpreted as little-endian,
// give the correct DWORD value.
// NOTE: This function is currently unused — DecryptBlock uses AddBits with ring buffers
// for correct MSB-first decoding. Kept for potential future use.
int SimpleModulusCipher::ReadBits(const void* buffer, int startBit, int numBits) {
    if (numBits == 0) return 0;
    
    const BYTE* buf = (const BYTE*)buffer;
    BYTE resultBuf[4] = {0};  // temp buffer, then reinterpret as DWORD
    
    // Read bits MSB-first into result buffer, matching ShiftBytes behavior
    for (int i = 0; i < numBits; i++) {
        int srcTotalBit = startBit + i;
        int srcByte = srcTotalBit / 8;
        int srcBitOff = 7 - (srcTotalBit % 8);  // MSB-first
        int bit = (buf[srcByte] >> srcBitOff) & 1;
        
        int dstTotalBit = i;  // write to result buffer starting at bit 0
        int dstByte = dstTotalBit / 8;
        int dstBitOff = 7 - (dstTotalBit % 8);  // MSB-first
        
        if (bit)
            resultBuf[dstByte] |= (1 << dstBitOff);
        // else: resultBuf is zero-initialized, no need to clear
    }
    
    // Reinterpret as little-endian DWORD (matches BitConverter.ToUInt32 on x86)
    return *(DWORD*)resultBuf;
}

// ============================================================================
// Decrypt: reverse of Encrypt
// Input: 11-byte encrypted block → Output: 8-byte plaintext
// ============================================================================
int SimpleModulusCipher::Decrypt(void* dest, const void* source, int size) {
    if (!m_initialized && !Initialize()) return -1;
    
    // Each 11-byte encrypted block → up to 8-byte plaintext
    int blockCount = size / SM_ENCRYPTED_SIZE;
    
    if (dest == NULL) return blockCount * SM_BLOCK_SIZE;
    
    BYTE* src = (BYTE*)source;
    BYTE* dst = (BYTE*)dest;
    int totalValidBytes = 0;
    
    for (int i = 0; i < blockCount; i++, src += SM_ENCRYPTED_SIZE, dst += SM_BLOCK_SIZE) {
        int validBytes = DecryptBlock(dst, src);
        if (validBytes < 0) {
            Logger::Log("SMOD", "Decrypt: block %d/%d checksum FAILED", i, blockCount);
            return -1;  // checksum failure
        }
        totalValidBytes = i * SM_BLOCK_SIZE + validBytes;  // last block's valid byte count
    }
    
    return totalValidBytes > 0 ? totalValidBytes : blockCount * SM_BLOCK_SIZE;
}

int SimpleModulusCipher::DecryptBlock(void* dest, const void* source) {
    DWORD* modulus = (DWORD*)&m_keys[0 * SM_KEY_COUNT];
    DWORD* decKey  = (DWORD*)&m_keys[2 * SM_KEY_COUNT];
    DWORD* xorKey  = (DWORD*)&m_keys[3 * SM_KEY_COUNT];
    
    // Guard: if keys are zero, don't crash
    if (modulus[0] == 0) {
        memset(dest, 0, SM_BLOCK_SIZE);
        return -1;
    }
    
    // Diagnostic logging — verbose for first 10 blocks, summary-only after
    static int dbgCount = 0;
    bool verbose = (dbgCount < 10);
    {
        const uint8_t* src = (const uint8_t*)source;
        Logger::Log("SMOD", "DecryptBlock #%d: src=[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]",
            dbgCount,
            src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7], src[8], src[9], src[10]);
        if (verbose) {
            Logger::Log("SMOD", "  Mod=[%08X %08X %08X %08X] Dec=[%08X %08X %08X %08X] Xor=[%08X %08X %08X %08X]",
                modulus[0], modulus[1], modulus[2], modulus[3],
                decKey[0], decKey[1], decKey[2], decKey[3],
                xorKey[0], xorKey[1], xorKey[2], xorKey[3]);
        }
        dbgCount++;
    }
    
    // Step 1: Unpack bits from 11-byte encrypted block into ring buffers
    // Matches C# Decode11To8: ShiftBytes into ringBuf, then BitConverter.ToUInt32
    BYTE ringBuf[4][4] = {{0}};
    AddBits(ringBuf[0], 0x00, source, 0x00, 0x10);  // ring0: 16 bits from offset 0
    AddBits(ringBuf[0], 0x16, source, 0x10, 0x02);  // ring0: 2 overflow bits at dest offset 0x16=22
    AddBits(ringBuf[1], 0x00, source, 0x12, 0x10);  // ring1: 16 bits from offset 18
    AddBits(ringBuf[1], 0x16, source, 0x22, 0x02);  // ring1: 2 overflow bits at dest offset 0x16=22
    AddBits(ringBuf[2], 0x00, source, 0x24, 0x10);  // ring2: 16 bits from offset 36
    AddBits(ringBuf[2], 0x16, source, 0x34, 0x02);  // ring2: 2 overflow bits at dest offset 0x16=22
    AddBits(ringBuf[3], 0x00, source, 0x36, 0x10);  // ring3: 16 bits from offset 54
    AddBits(ringBuf[3], 0x16, source, 0x46, 0x02);  // ring3: 2 overflow bits at dest offset 0x16=22
    
    DWORD ring[4];
    for (int r = 0; r < 4; r++)
        ring[r] = *(DWORD*)ringBuf[r];  // little-endian reinterpret, matches BitConverter.ToUInt32
    
    // Log ringBuf byte contents (compare with C# ShiftBytes output)
    if (verbose) {
        Logger::Log("SMOD", "  ringBuf[0]=[%02X %02X %02X %02X] ringBuf[1]=[%02X %02X %02X %02X]",
            ringBuf[0][0], ringBuf[0][1], ringBuf[0][2], ringBuf[0][3],
            ringBuf[1][0], ringBuf[1][1], ringBuf[1][2], ringBuf[1][3]);
        Logger::Log("SMOD", "  ringBuf[2]=[%02X %02X %02X %02X] ringBuf[3]=[%02X %02X %02X %02X]",
            ringBuf[2][0], ringBuf[2][1], ringBuf[2][2], ringBuf[2][3],
            ringBuf[3][0], ringBuf[3][1], ringBuf[3][2], ringBuf[3][3]);
    }
    
    // Step 2: Read checksum (16 bits at offset 0x48 = 72)
    BYTE finale[2] = {0};
    AddBits(finale, 0, source, 0x48, 0x10);  // 16 bits for finale
    
    // Log ring DWORD values and finale
    Logger::Log("SMOD", "  ring=[%08X %08X %08X %08X] finale=[%02X %02X]",
        ring[0], ring[1], ring[2], ring[3], finale[0], finale[1]);
    
    // Step 3: Reverse XOR chain (matches C# Decode11To8)
    ring[2] ^= xorKey[2] ^ (ring[3] & 0xFFFF);
    ring[1] ^= xorKey[1] ^ (ring[2] & 0xFFFF);
    ring[0] ^= xorKey[0] ^ (ring[1] & 0xFFFF);
    
    // Log ring values after XOR chain
    if (verbose) {
        Logger::Log("SMOD", "  ringAfterXOR=[%08X %08X %08X %08X]",
            ring[0], ring[1], ring[2], ring[3]);
    }
    
    // Step 4: Reverse pass 1 — modular multiplication and XOR
    // Use uint64_t for multiplication to match C# uint*uint behavior
    // C# uint*uint truncates to 32 bits BEFORE modulus, so we test both
    DWORD mult[4];
    for (int i = 0; i < 4; i++) {
        uint64_t product64 = (uint64_t)ring[i] * decKey[i];
        mult[i] = (DWORD)(product64 % modulus[i]);
        if (verbose) {
            DWORD mult32 = (ring[i] * decKey[i]) % modulus[i];
            Logger::Log("SMOD", "  slot[%d]: ring=%08X * dec=%08X mod mod=%08X = %08X (32bit=%08X product64=%016llX)",
                i, ring[i], decKey[i], modulus[i], mult[i], mult32, (unsigned long long)product64);
        }
    }
    
    BYTE plaintext[SM_BLOCK_SIZE];
    plaintext[0] = (BYTE)(xorKey[0] ^ mult[0]);
    plaintext[1] = (BYTE)((xorKey[0] ^ mult[0]) >> 8);
    plaintext[2] = (BYTE)(xorKey[1] ^ mult[1] ^ (ring[0] & 0xFFFF));
    plaintext[3] = (BYTE)((xorKey[1] ^ mult[1] ^ (ring[0] & 0xFFFF)) >> 8);
    plaintext[4] = (BYTE)(xorKey[2] ^ mult[2] ^ (ring[1] & 0xFFFF));
    plaintext[5] = (BYTE)((xorKey[2] ^ mult[2] ^ (ring[1] & 0xFFFF)) >> 8);
    plaintext[6] = (BYTE)(xorKey[3] ^ mult[3] ^ (ring[2] & 0xFFFF));
    plaintext[7] = (BYTE)((xorKey[3] ^ mult[3] ^ (ring[2] & 0xFFFF)) >> 8);
    
    // Log plaintext bytes
    if (verbose) {
        Logger::Log("SMOD", "  plaintext=[%02X %02X %02X %02X %02X %02X %02X %02X]",
            plaintext[0], plaintext[1], plaintext[2], plaintext[3],
            plaintext[4], plaintext[5], plaintext[6], plaintext[7]);
    }
    
    // Step 5: Validate checksum
    finale[0] ^= finale[1];
    finale[0] ^= 0x3D;
    
    BYTE m = 0xF8;
    for (int k = 0; k < 8; k++) m ^= plaintext[k];
    Logger::Log("SMOD", "  checksum calc=0x%02X expected=0x%02X numBytes=%d",
        m, finale[1], finale[0]);
    if (m != finale[1]) {
        // Checksum mismatch — decryption failed
        Logger::Log("SMOD", "DecryptBlock: checksum FAILED (calc=0x%02X expected=0x%02X) numBytes=%d",
            m, finale[1], finale[0]);
        
        // Re-read SM Object keys from memory and compare against cached keys
        uintptr_t base = GameMemory::Globals::MainBase();
        if (base) {
            uintptr_t obj = base + SM_OBJECT_OFFSET;
            __try {
                DWORD prefix = *(DWORD*)(obj + OFF_PREFIX);
                Logger::Log("SMOD", "SM object prefix: 0x%08X (expected 0x00D546EC)", prefix);
                DWORD* curModulus = (DWORD*)(obj + OFF_MODULUS);
                DWORD* curEncKey  = (DWORD*)(obj + OFF_ENCKEY);
                DWORD* curXorKey  = (DWORD*)(obj + OFF_XORKEY);
                Logger::Log("SMOD", "Live Mod=[%08X %08X %08X %08X] Cached=[%08X %08X %08X %08X]",
                    curModulus[0], curModulus[1], curModulus[2], curModulus[3],
                    m_keys[0], m_keys[1], m_keys[2], m_keys[3]);
                Logger::Log("SMOD", "Live Enc=[%08X %08X %08X %08X] Cached=[%08X %08X %08X %08X]",
                    curEncKey[0], curEncKey[1], curEncKey[2], curEncKey[3],
                    m_keys[4], m_keys[5], m_keys[6], m_keys[7]);
                Logger::Log("SMOD", "Live Xor=[%08X %08X %08X %08X] Cached=[%08X %08X %08X %08X]",
                    curXorKey[0], curXorKey[1], curXorKey[2], curXorKey[3],
                    m_keys[12], m_keys[13], m_keys[14], m_keys[15]);
                
                bool keysChanged = false;
                for (int i = 0; i < SM_KEY_COUNT; i++) {
                    if (curModulus[i] != m_keys[i] || curEncKey[i] != m_keys[4+i] || curXorKey[i] != m_keys[12+i]) {
                        keysChanged = true;
                        break;
                    }
                }
                
                if (keysChanged) {
                    Logger::Log("SMOD", "Keys changed! Re-initializing...");
                    m_initialized = false;
                    Initialize();
                } else {
                    Logger::Log("SMOD", "Keys unchanged — algorithm or data issue");
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("SMOD", "Failed to re-read keys from memory");
            }
        }
        
        // Roundtrip self-test: encrypt known data, then decrypt with same keys
        // Guard against infinite recursion (DecryptBlock → self-test → Encrypt → Decrypt → DecryptBlock)
        static int selfTestDepth = 0;
        if (selfTestDepth == 0) {
            selfTestDepth++;
            uint8_t testData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
            uint8_t encBuf[11], decBuf[8];
            int encLen = Encrypt(encBuf, testData, 8);
            if (encLen > 0) {
                int decLen = Decrypt(decBuf, encBuf, encLen);
                if (decLen > 0 && memcmp(testData, decBuf, 8) == 0) {
                    Logger::Log("SMOD", "Self-test: PASS (algorithm OK, likely key mismatch or wrong SM object)");
                } else {
                    Logger::Log("SMOD", "Self-test: FAIL (algorithm bug, decLen=%d)", decLen);
                }
            } else {
                Logger::Log("SMOD", "Self-test: ENCRYPT FAILED (encLen=%d)", encLen);
            }
            selfTestDepth--;
        } else {
            Logger::Log("SMOD", "Self-test: SKIPPED (recursive call, depth=%d)", selfTestDepth);
        }
        
        memset(dest, 0, SM_BLOCK_SIZE);
        return -1;
    }
    
    memcpy(dest, plaintext, SM_BLOCK_SIZE);
    return finale[0];  // return number of valid bytes (1-8)
}

// ============================================================================
// ModInverse32: compute per-DWORD modular inverse.
// SimpleModulus uses per-DWORD modular multiplication:
//   ring[i] = (temp * encKey[i]) % modulus[i]
// Decryption requires: (ring[i] * decKey[i]) % modulus[i] = temp
// So decKey[i] must be the modular inverse: (encKey[i] * decKey[i]) % modulus[i] == 1
// Uses extended Euclidean algorithm for 32-bit integers.
// ============================================================================
DWORD SimpleModulusCipher::ModInverse32(DWORD a, DWORD m) {
    // Extended Euclidean Algorithm to find x such that (a * x) % m == 1
    // All arithmetic fits in 64-bit since a, m < 2^32
    int64_t old_r = (int64_t)a;
    int64_t r = (int64_t)m;
    int64_t old_s = 1;
    int64_t s = 0;
    
    while (r != 0) {
        int64_t q = old_r / r;
        int64_t new_r = old_r - q * r;
        int64_t new_s = old_s - q * s;
        old_r = r;
        old_s = s;
        r = new_r;
        s = new_s;
    }
    
    // old_r should be gcd(a, m) == 1 for prime modulus
    // old_s is the Bezout coefficient; modular inverse is old_s mod m
    int64_t result = old_s % (int64_t)m;
    if (result < 0) result += m;
    return (DWORD)result;
}
