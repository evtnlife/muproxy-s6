# Plan: Debug C3 Decryption (SimpleModulus)

## Problem Statement
SimpleModulus decryption returns -1 (checksum failure) on all C3 packets. After fixing MSB-first bit ordering and rewriting DecryptBlock/EncryptBlock to use ring buffers matching C#/navossoc, the build succeeded but checksum still fails: `calc=0x7D expected=0xD5 numBytes=6`.

## Root Cause Candidates
1. **Stale/wrong keys** — SM Object keys at runtime may differ from what we're reading
2. **Algorithm step mismatch** — Some subtle difference between our C++ DecryptBlock and the C# reference
3. **XOR chain order** — Possible difference in XOR chain application
4. **Overflow bit packing** — Overflow bits may not be where we think they are
5. **Key initialization** — Our computed DecKey may not match what the game uses

## Key Facts (Confirmed)
- **SM Object 1** (C2S encrypt) at `0x08797778`, 72 bytes total
- **Object layout**: +0x00: DWORD prefix, +0x04: Modulus[4], +0x14: EncKey[4], +0x24: DecKey[4] (zeros!), +0x34: XorKey[4], +0x44: DWORD flags
- **Keys**: Mod=[0x1F44F, 0x28386, 0x1125B, 0x1A192], Enc=[0x5BC1, 0x2E87, 0x4D68, 0x354F], Xor=[0xBD1D, 0xB455, 0x3B43, 0x9239], Dec=[0x7B38, 0x7FF, 0xDEB3, 0x27C7] (per-DWORD mod inverse)
- **MSB-first bit ordering** confirmed correct (bit offset 0 = bit 7 of byte[0])
- **NativeDecrypt always fails for C→S** — Obj1 DecKey=zeros by design
- **Per-DWORD mod inverse** verified: `encKey[i] * decKey[i] % modulus[i] == 1` for all i

## Code Status
- `gg_bypass2/src/SimpleModulusCipher.cpp` — AddBits/ReadBits rewritten MSB-first, DecryptBlock/EncryptBlock use ring buffers
- `gg_bypass2/src/PacketSniffer.cpp` — SEND hook applies GameServerFullDecrypt then SM Decrypt

## Action Items

### Task 1: Add Diagnostic Logging to DecryptBlock
- Print ring values after unpacking (from AddBits into ringBuf)
- Print ring values after XOR chain
- Print multiplication intermediates: `(ring[i] * decKey[i]) % modulus[i]`
- Print plaintext bytes (c0-c3)
- Print live key comparison: read SM Object keys at decrypt time vs our stored keys
- Log the raw source bytes being fed to DecryptBlock
- Log the checksum calculation: computed vs expected

### Task 2: x64dbg Dynamic Analysis
- Set hardware breakpoints at SM Object key offsets:
  - `0x0879777C` (Modulus start)
  - `0x0879778C` (EncKey start) 
  - `0x087977AC` (XorKey start)
- Monitor who writes to the SM Object — detect if keys change at runtime
- Step through DecryptBlock in x64dbg to verify ring values match manual trace

### Task 3: Verify Roundtrip in Initialize()
- The Encrypt→Decrypt self-test should pass if algorithm is correct
- If self-test fails: bug in algorithm
- If self-test passes but real packets fail: keys may be stale or wrong

### Task 4: Compare C# Decode11To8 vs C++ DecryptBlock Bit-by-Bit
- Use the test_decode.cs reference implementation
- Feed identical packet bytes to both C# and C++ implementations
- Compare ringBuf values at each step
- Compare intermediate values after XOR chain
- Compare final plaintext output

### Task 5: Verify C# ShiftBytes vs C++ AddBits Equivalence
- Specifically test the overflow bits (bit offsets 0x10, 0x22, 0x34, 0x46 with length 0x02)
- These 2-bit overflow values go into DWORD bits 22-23 (byte[2] bits 7-6 in MSB-first)
- Verify ringBuf byte content matches between C# and C++ for same input

## Technical Reference

### C# ShiftBytes → C++ AddBits Mapping
```
C#:  ShiftBytes(ringBuf[0], 0x00, src, 0x00, 0x10)
C++: AddBits(ringBuf[0], 0,    source, 0x00, 0x10)

C#:  ShiftBytes(ringBuf[0], 0x16, src, 0x10, 0x02)  
C++: AddBits(ringBuf[0], 16,   source, 0x10, 0x02)
```

### DecryptBlock XOR Chain
```cpp
ring[2] ^= xorKey[2] ^ (ring[3] & 0xFFFF);
ring[1] ^= xorKey[1] ^ (ring[2] & 0xFFFF);
ring[0] ^= xorKey[0] ^ (ring[1] & 0xFFFF);
```

### DecryptBlock Plaintext Computation
```cpp
plaintext[i] = xorKey[i] ^ ((ring[i] * decKey[i]) % modulus[i]) ^ (ring[i-1] & 0xFFFF)
// where ring[-1] uses the XOR-chain-modified ring[i-1], not original
```

### Manual Trace (Packet CF 9F 73 A3 66 E9 62 7B DC EE D5)
- ring[0] = 0x00409FCF, ring[1] = 0x00808DCE, ring[2] = 0x0040E966, ring[3] = 0x0000F79E
- These were verified correct against MSB-first AddBits

## Priority Order
1. ⬜ Create diagnostic logging in DecryptBlock
2. ⬜ Build, deploy, and capture logs from a real C3 packet
3. ⬜ x64dbg memory monitoring of SM Object keys
4. ⬜ Compare C# vs C++ output for identical input
5. ⬜ Fix root cause based on diagnostic findings