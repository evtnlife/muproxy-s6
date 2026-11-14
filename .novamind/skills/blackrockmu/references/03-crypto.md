# 3. Crypto

> **Golden rule:** the one who decrypts is `byp.dll`. `MuProxy2` (C#) **never** touches crypto.

## Layer-1 (game server, applied to ALL bytes)
```
decrypt: plain[i] = (wire[i] ^ 0x36) - 0x07
encrypt: wire[i]  = (plain[i] + 0x07) ^ 0x36
```
- Keys: `Main.dll+0x403F75` (sub = `0x07`), `Main.dll+0x403F76` (xor = `0x36`).
- byp.dll fns: `GameServerDecryptLayer1` / `GameServerEncryptLayer1` (PacketSniffer.cpp).

## Camada-2 / XorData (chained XOR, on the DATA bytes `[3, len)` of C1 packets)
```
for n in [3, len):  buf[n] ^= (buf[n-1] ^ XorFilter[n % 32])
```
`XorFilter[32]` (read from `Main.dll 0x7166FF20` via x32dbg):
```
AB 11 CD FE 18 23 C5 A3  CA 33 C1 CC 66 67 21 F3
32 12 15 35 29 FF FE 1D  44 EF CD 41 26 3C 4E 4D
```
- Applied by the game on append (flag=1): `Append 0x404C10` → `XorData 0x404C90`.
- **NOT self-inverse in place** — decryption must use the *encrypted* previous byte, not the
  decrypted one.
- ⚠️ Older docs claimed "BlackRock doesn't use XorFilter" — that was **WRONG**. It IS applied to
  attack data bytes.
- byp.dll fn: `GameServerXorData(data, start, end)`.

## Full chain for a C1 attack
```
TRUE plaintext  →  XorData [3,len)  →  Layer-1  →  WIRE
```

## Decryption gotcha (display vs real plaintext)
- `GameServerFullDecrypt` only does Layer-1 (used for the log display). That returns the
  **XorData form** (e.g. `C1 07 11 EF ED B6 77`), NOT the real plaintext
  (`C1 07 11 00 1A 78 04`).
- To get the real plaintext: Layer-1 decrypt, THEN apply `GameServerXorData` over `[3, len)`
  (it is its own inverse).

## BlackRock does NOT use C1C2 XOR
- No C1C2 header XOR (table `0xE7,0x6D…`). Evidence: movement `C1 08 D4 02` shows opcode `D4`
  after Layer-1 (would be `8A` if C1C2 were used); C3 decrypt without XOR gives opcode `0x19`
  correctly. `MuEncDec.DecodeC1C2` is disabled (`DisableC1C2Xor=true`).
- `XorFilter` (Camada-2) is **NOT** used on the C3/C4 path (that uses SimpleModulus), but **IS**
  used on C1 `0x11` attacks.

## C3 / SimpleModulus (skill attacks)
- C3 packets use a modulus-based cipher (SimpleModulus). byp.dll implements it in
  `SimpleModulusCipher.cpp`.
- SM object: `main.exe+0x8397778` (RVA), abs `0x08797778` (confirmed via x64dbg at `0x404E73`).
  72 bytes:

  | Offset | Field | Note |
  |--------|-------|------|
  | +0x00 | DWORD prefix | always `0x00D546EC` (unused by encrypt/decrypt) |
  | +0x04 | DWORD Modulus[4] | 16 bytes |
  | +0x14 | DWORD Key[4] | EncKey for Obj1 |
  | +0x24 | DWORD Key2[4] | DecKey for Obj1 — **zeros by design** |
  | +0x34 | DWORD XorKey[4] | XOR keys for the chain cipher |
  | +0x44 | DWORD flags | 0 or 1 |

- Encrypt uses `Key@+0x14` + `XorKey@+0x34`. Decrypt uses `Key2@+0x24` (DecKey) + `XorKey@+0x34`.
- For C2S, Obj1 has `EncKey@+0x14` and `DecKey=zeros@+0x24`, so the Dec1 keys must be computed as
  the 128-bit modular inverse of the Enc1 keys (`ModInverse32`).
- Block: 8 bytes in → 11 bytes out (`SM_BLOCK_SIZE=8`, `SM_ENCRYPTED_SIZE=11`).
- Bit packing is **MSB-first** (matches the game's `ShiftBytes`). Checksum:
  `finale[1]=0xF8` XOR'd over the source bytes; `finale[0]=(size ^ 0x3D) ^ finale[1]`.
- Native path: `NativeEncrypt` calls the game's `SimpleModulus::Encrypt` at `0x998420`;
  `NativeDecrypt` calls `0x998490` (100% identical to the game).
- **`NativeDecrypt` always fails for C→S** (Obj1 DecKey = zeros by design) — don't chase it; it's
  not a bug.

## Skill attack opcodes
- Two skill opcodes exist (`MuOpcodes.h`):
  - `0x19` **TARGETED_SKILL** — the **implemented/validated** path (builder internal, C3
    encrypted). See §4.
  - `0xE5` **SKILL_ATTACK** — 13-byte C3 skill attack (legacy). Older notes described it as a
    6-byte chained-XOR key (`CD 41 26 3C 4E 4D` at `main.exe+0x404D01`, encrypt fn `0x404DB0`) —
    treat as unconfirmed; the working path is `0x19` + SimpleModulus.
- **Skill attacks use C3 (SimpleModulus), NOT Layer-1 XorData.** Don't reuse Layer-1 for skills.
