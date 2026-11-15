# Native Attack Packet (Opcode 0x11) — Reverse Engineering Report (VALIDATED)

> **Status: VALIDATED.** The full two-layer encryption chain reproduces the live wire
> capture byte-for-byte, and the reconstructed plaintext matches the disassembly
> constants (`push 0x78`, D3 range 0–7) across **4 independent captures**.
> This document **corrects** the earlier draft (wrong plaintext, wrong decrypt order,
> and a false "no XorFilter" conclusion).

---

## 1. Executive Summary (the corrected picture)

The native attack packet is built by `main.exe` at `0x5A1701` and passes through
**two encryption layers** before hitting the wire:

```
TRUE plaintext  →  XorData (Camada 2) on data bytes  →  Layer-1 on whole packet  →  WIRE
C1 07 11 00 1A 78 04        [3,7)                     (c+0x07)^0x36
        C1 07 11 EF ED B6 77                                  FE 38 2E C0 C2 8B 48
```

| Layer | Where | Formula | Scope |
|-------|-------|---------|-------|
| **Camada 2 (XorData)** | `Append 0x404C10` → `0x404C90` (on append, flag=1) | `buf[n] ^= (buf[n-1] ^ XorFilter[n%32])` | data bytes `[3,7)` only |
| **Camada 1 (Layer-1)** | game socket layer (location TBD, §7) | `wire = (plain + 0x07) ^ 0x36` | whole packet |

**byp.dll does NOT encrypt.** `HookSend` (PacketSniffer.cpp:827) forwards the game's
already-encrypted buffer to `TrueSend` unchanged; it only decrypts a **copy** (Layer-1
only) for display. So the "plaintext" byp logs (`C1 07 11 EF ED B6 77`) is actually the
**XorData-encrypted intermediate**, not the true plaintext.

---

## 2. Packet Structure (TRUE plaintext)

```
C1 07 11 [D0] [D1] [D2] [D3]
│  │  │   │    │    │    │
│  │  │   │    │    │    └─ D3: 0..7   (from [ebp-0x14] & 0x80000007) — varies per attack
│  │  │   │    │    └────── D2: 0x78   (CONSTANT — matches disasm `push 0x78`)
│  │  │   │    └─────────── D1: target low byte (1A, 1B, 25, ...)
│  │  │   └──────────────── D0: target high byte (always 0x00 → target < 256)
│  │  └──────────────────── opcode 0x11 (attack)
│  └─────────────────────── length = 7
└────────────────────────── magic C1
```

### True-plaintext reconstruction (XorData-decrypt `[3,7)` of each capture)

| # | Capture (XorData-enc form) | D0 | D1 | D2 | D3 | D2=78? | D3 in 0–7? |
|---|----------------------------|----|----|----|----|--------|-----------|
| 1 | `C1 07 11 EF EC B7 74` | 00 | 1B | **78** | 06 | ✅ | ✅ |
| 2 | `C1 07 11 EF D2 89 4F` | 00 | 25 | **78** | 03 | ✅ | ✅ |
| 3 | `C1 07 11 EF D2 89 48` | 00 | 25 | **78** | 04 | ✅ | ✅ |
| 4 | live `C1 07 11 EF ED B6 77` | 00 | 1A | **78** | 04 | ✅ | ✅ |

> **Decisive:** D2 = `0x78` in **every** capture (matches the disasm constant), and D3 is
> always in 0–7 (matches the disasm range). The old MD's "D2/D3 discrepancy" was because
> it read the XorData-encrypted bytes, not the true plaintext.

### Target ID

- `D0 D1` = target index bytes. Live capture: `00 1A`.
- **CORREÇÃO (validada em campo, 24 capturas):** opcode 0x11 usa o **entityIndex PURO**
  em D0D1 — **SEM XOR 0xEFF5**. O builder nativo coloca o índice cru (monstro 0x001A →
  D0D1 = `00 1A` em todas as capturas, nos 4 alvos 0x001A/0x001B/0x0025/0x005A).
  O XOR 0xEFF5 é convenção de **outros opcodes** (skill 0x19 lê o global 0x081C03A4
  que já armazena o valor XORed), não do builder 0x11.
- ⚠️ **Implication:** byp.dll / MuProxy2 read the target from the **Layer-1-only**
  decrypt (`EF ED`), missing the XorData layer. Their target tracking is therefore
  **off** — the true target requires XorData-decrypt first (and yields the RAW index,
  e.g. `00 1A`, not `0xEFEF`).

---

## 3. Encryption — Full Verification (live capture)

### Layer-1 (Camada 1) — per-byte, whole packet

```
encrypt: wire  = (plain + 0x07) ^ 0x36
decrypt: plain = (wire  ^ 0x36) - 0x07
```
Keys `0x36` / `0x07` confirmed at `Main.dll+0x403F75/76` (byp `GetGameServerKeys`).

### XorData (Camada 2) — chained XOR, data bytes `[3,7)`

```
for n in [3,7):  buf[n] ^= (buf[n-1] ^ XorFilter[n % 32])
```
`XorFilter[32]` (loaded in `0x404C90`, identical to byp `g_XorFilter`):
```
AB 11 CD FE 18 23 C5 A3  CA 33 C1 CC 66 67 21 F3
32 12 15 35 29 FF FE 1D  44 EF CD 41 26 3C 4E 4D
```

### Step-by-step reproduction (live)

| Step | Bytes | Note |
|------|-------|------|
| TRUE plaintext | `C1 07 11 00 1A 78 04` | builder output |
| after XorData `[3,7)` | `C1 07 11 EF ED B6 77` | `00^(11^FE)=EF`, `1A^(EF^18)=ED`, `78^(ED^23)=B6`, `04^(B6^C5)=77` |
| after Layer-1 | `FE 38 2E C0 C2 8B 48` | **= actual wire ✅** |

Every byte matches. **The chain is proven.**

---

## 4. Send Path (full call chain, all disassembled)

```
Builder 0x5A1701 (main.exe)
  ├─ WriteHeader 0x404B90 (C1, 0x11)            → appends C1, len-placeholder, 0x11
  ├─ AppendByte 0x4050D0  ×4 (D0,D1,D2,D3)
  │     └─ Append 0x404C10(this,&byte,1,flag=1)
  │           ├─ memcpy into buf[word0+2]        (0x404C44)
  │           └─ [flag==1] → XorData 0x404C90    ← CAMADA 2 applied here (data bytes)
  └─ Packet::Send 0x404D70
        ├─ Finalize 0x404FA0                     → patches len byte (memcpy(this+3,this,1))
        ├─ Encrypt 0x405020                      → NO-OP (add eax,2; ret)
        └─ SendRaw 0x404DB0
              ├─ [flag==0] simple path → 0x405110
              └─ [flag!=0] C3/C4 frame (SimpleModulus 0x998420) — NOT used for this C1 attack
                    └─ 0x405110 → call [0xD227F8] (= ws2_32:send, hooked)
                          └─ byp.dll HookSend (PacketSniffer.cpp:670)
                                ├─ decrypts a COPY (Layer-1 only) for display
                                ├─ LogRaw("SEND", s, buf, len)  ← wire bytes
                                └─ return TrueSend(s, buf, len)  ← forwards ORIGINAL (line 827)
```

### Packet object layout

```
+0x00  word0 = length counter (low byte copied to buffer[1] by Finalize)
+0x02  data[0] = C1 (magic)
+0x03  data[1] = length byte
+0x04  data[2] = opcode (0x11)
+0x05  data[3..] = D0 D1 D2 D3 ...
```

### Key disassembly findings

| Address | Function | Role | Layer-1? |
|---------|----------|------|----------|
| `0x5A1701` | Builder | pushes C1, 0x11, D0, D1, `0x78`, D3 | no (plaintext) |
| `0x4050D0` | AppendByte | `Append(this,&b,1,1)` | no |
| `0x404C10` | Append | memcpy + **XorData if flag=1** | **Camada 2 here** |
| `0x404C90` | XorData | chained XOR w/ 32-byte table | Camada 2 |
| `0x404FA0` | Finalize | patches length byte | no |
| `0x405020` | Encrypt | **no-op** (`add eax,2; ret`) | no |
| `0x404DB0` | SendRaw | C3/C4 framing or simple path | no |
| `0x405110` | low-level send | `call [0xD227F8]` (hooked send) | no |

---

## 5. Corrections to the earlier draft

| Item | Old draft (WRONG) | Validated (CORRECT) |
|------|-------------------|---------------------|
| Layer-1 decrypt | `(wire - 0x07) ^ 0x36` | `(wire ^ 0x36) - 0x07` |
| Plaintext | `C1 07 11 8F 8D B2 75` | `C1 07 11 00 1A 78 04` |
| Camada 2 (XorData) | "NOT present in BlackRock" | **IS applied** to data bytes `[3,7)` on append |
| D2 byte | "discrepancy: 0x78 vs B6" | **Resolved:** true D2 = `0x78`; `B6` is its XorData form |
| D3 byte | "discrepancy: 0–7 vs 75/74/4F/48" | **Resolved:** true D3 ∈ 0–7; captured values are XorData forms |
| "byte-3 discrepancy C2 vs C0" | flagged as open | **Artifact** of the wrong decrypt order — gone |
| byp.dll role | ambiguous | **Forwards original buffer** (line 827); only decrypts a copy for display |

---

## 6. Modules

| Module | Base | Role |
|--------|------|------|
| `main.exe` | 0x400000 | Game client (builder, Append/XorData, send path) |
| `Main.dll` | 0x6E690000 | Game logic (attack state machine, keys at +0x403F75/76) |
| `byp.dll` | 0x70060000 | Bypass (hooks ws2_32:send; sniffs/displays; does NOT encrypt) |
| `ws2_32.dll` | 0x76860000 | Winsock (`send` at 0x768758A0 is hooked) |

---

## 7. Open Questions / To Verify

| # | Question | Status |
|---|----------|--------|
| 1 | **Exact location of Layer-1** `(c+0x07)^0x36`. Not in Append/AppendByte/Finalize/Encrypt/SendRaw/0x405110. Likely in the socket-flush layer behind `[0xD227F8]` or a WSASend path. | 🟡 Set BP at `0x405110`/`[0xD227F8]`, compare buffer in vs. wire |
| 2 | **Target ID XOR:** is it `(D0D1)^0xEFF5` (→ 0xEFEF) or raw (→ 0x001A=26)? D0 is always 0x00. | 🟡 Cross-ref with D4/movement packets & server OBJECT_RANGE |
| 3 | **byp target tracking bug:** byp reads target from Layer-1-only decrypt (misses XorData). Confirm it reports the wrong target and fix by adding XorData-decrypt. | 🟡 Verify in MuProxy2 |
| 4 | **Movement packets (C1 08 D4 02):** byp claims no XorData for them. Do they use flag=0 append? | ⚪ Disasm the movement builder |
| 5 | **Attack trigger** `0x6E7D871F` (Main.dll state machine). | ⚪ Not started |

---

## 8. Next Steps

1. **Confirm Layer-1 location:** BP at `0x405110` entry and at the hooked `send`; dump the
   buffer at both — the transition from XorData-form to wire-form pinpoints Layer-1.
2. **Fix byp/MuProxy2 target extraction:** add XorData-decrypt `[3,7)` after Layer-1 so the
   true target (`00 1A` → `0xEFEF`) is reported, not the intermediate (`EF ED`).
3. ~~**Build a synthetic attack packet** for injection~~ — **DONE** (see §8b).
4. **Disasm the movement builder** to confirm whether Camada 2 is skipped (flag=0) for
   non-attack packets, reconciling the byp "no XorFilter" note.

---

## 8b. Code Fix Applied (attack rule)

The attack rule in `gg_bypass2` was corrected to match the validated two-layer chain.
Four bugs were found and fixed:

| # | File:line | Bug | Fix |
|---|-----------|-----|-----|
| 1 | `AttackModule.cpp` `BuildAttackPacket` | D2 (byte 5) = `ComputeActionByte()` = `0x04` | D2 = `0x78` constant (disasm `push 0x78`) |
| 2 | `AttackModule.cpp` `ComputeDirection` | returned `0xC0\|dir` (0xC0–0xC7) | returns raw `0..7` (disasm `& 0x80000007`) |
| 3 | `PacketSniffer.cpp` `InjectToServer` | applied **only** Layer-1 (missing XorData) | new `InjectC1Attack` applies XorData `[3,len)` **then** Layer-1 |
| 4 | `AttackModule.cpp` `BuildAttackPacket` | D0D1 = `entityIndex ^ 0xEFF5` (e.g. `EF EF`) | D0D1 = **raw entityIndex** (e.g. `00 1A`) — see below |

**Bug 4 evidence (field, 2026-11-13):** XorData-decrypting **all 24** native attack
captures in `byp_packets.log` (targets 0x001A/0x001B/0x0025/0x005A, dirs 02/03/04/06)
gives D2 = `0x78` in **every** one and D0D1 = the **raw tracker index** (0x001A etc.).
The injected attacks (gg_bypass2.log 18:18) sent D0D1 = `EF EF` (XORed) for the same
monster 0x001A the native attack hit with `00 1A` — the server XorData-decrypts to
`EF EF` → entity 0xEFEF (nonexistent) → **all injected attacks ignored**.
The old DLL "worked on some monsters" because its no-XorData XORed target
(`EF EF` raw on wire) XorData-decrypted to `00 18` — the **adjacent** entity, which
happened to be a live monster in dense spawns.

**Key subtlety:** `GameServerXorData` (in-place chained XOR) is **not self-inverse** —
decrypting requires the *encrypted* previous byte, not the decrypted one. So the two
injection paths pass **different forms** and must not share the XorData step:

| Caller | Input form | Function | Layers applied |
|--------|-----------|----------|----------------|
| `SendAttack` → `BuildAttackPacket` | TRUE plaintext `C1 07 11 00 1A 78 04` | `InjectC1Attack` | XorData + Layer-1 |
| `HandleSendPacket` (user hex) | XorData form `C1 07 11 ED 1F 44 86` | `InjectToServer` | Layer-1 only |

**Verification (REPL):** `InjectC1Attack` on the live target (`entityIndex=0x001A`)
reproduces the wire capture byte-for-byte:
`C1 07 11 00 1A 78 04` → XorData → `C1 07 11 EF ED B6 77` → Layer-1 → `FE 38 2E C0 C2 8B 48` ✅

Build: `gg_bypass2/build.bat` compiles clean (0 errors) and deploys `byp.dll`.

---

## 9. Files Referenced

| File | Purpose |
|------|---------|
| `F:\blackrockmu\byp_packets.log` | Captured packet log (Layer-1-decrypted form) |
| `f:\blackrockmu\byp\gg_bypass2\src\PacketSniffer.cpp` | Keys, XorFilter table, HookSend (line 827), Layer-1 fns |
| `f:\blackrockmu\byp\MuProxy2\MuProxy2\TcpLoopbackProxy.cs` | Proxy |
| `f:\blackrockmu\byp\MuProxy2\MuProxy2\Protocol\Messages.cs` | C# message defs |

### Disassembly evidence (x32dbg, main.exe)

| Address | Finding |
|---------|---------|
| `0x5A1701` | Builder: `push 0x11; push 0xC1` (header), `push 0x78` (D2), D3 from `[ebp-0x14]` |
| `0x404C10` | Append: memcpy then `[flag==1] → call 0x404C90` |
| `0x404C90` | XorData: loads 32-byte table, `buf[n] ^= (buf[n-1] ^ table[n%32])` |
| `0x404FA0` | Finalize: `memcpy(this+3, this, 1)` patches length |
| `0x405020` | Encrypt: `add eax,2; ret` (no-op) |
| `0x404DB0` | SendRaw: C3/C4 framing (SimpleModulus 0x998420) or simple path |
| `0x405110` | low-level: `call [0xD227F8]` (hooked ws2_32:send) |
