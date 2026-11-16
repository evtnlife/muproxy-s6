# 4. Attack packets

## Normal attack 0x11 (VALIDATED — source of truth: `attack_packet_re.md`)
```
TRUE plaintext:  C1 07 11 [D0] [D1] 78 [D3]
   D0 D1 = raw entityIndex (big-endian, NO XOR 0xEFF5)
   D2    = 0x78  (constant — disasm `push 0x78`)
   D3    = 0..7  (raw direction — `[ebp-0x14] & 0x80000007`)
```
Chain (reproduces the live wire byte-for-byte):
```
C1 07 11 00 1A 78 04  → XorData [3,7) →  C1 07 11 EF ED B6 77  → Layer-1 →  FE 38 2E C0 C2 8B 48  (wire)
```
- D0 is always `0x00` in observed captures (target < 256).
- D3 direction formula (`AttackModule::ComputeDirection`):
  `dir = floor((atan2(dy,dx) + PI/2) / (2*PI) * 8) % 8` → N=0, NE=1, E=2, SE=3, S=4, SW=5, W=6, NW=7.
  Pure 0..7, **no 0xC0 mask**. (The old "0xC5 (SW)" note was just the XorData form of the real
  `D3=0x04`.)

## Injection paths (must NOT share the XorData step or you double-encrypt)
| Caller | Input form | Function | Layers applied |
|--------|-----------|----------|----------------|
| `SendAttack` → `BuildAttackPacket` | TRUE plaintext `C1 07 11 00 1A 78 04` | `PacketInjector::Inject` | XorData `[3,len)` **then** Layer-1 |
| `SendSkillAttack` | TRUE plaintext `C1 07 19 00 3C 02 77` | `PacketInjector::Inject` | XorData `[3,len)` → SM → `C3 0D [11]` → Layer-1 |
| `HandleSendPacket` (user hex) | XorData form `C1 07 11 ED 1F 44 86` | `PacketInjector::InjectXorData` | Layer-1 only |

- `PacketInjector::Inject` (`packets/PacketInjector.cpp`) is the **only** injection entry: it
  validates `C1` + opcode (`0x11` or `0x19`, 7 bytes), applies `MuCipher::XorData(buf, 3, len)`
  **exactly once**, picks the pipeline by opcode and hands the ciphered bytes to
  `GameServerConnection::SendToGameServer` (the socket owner).
- **Never** call `MuCipher::XorData` yourself before `Inject` — Camada-2 would be applied twice.
- The `0xEFF5` XOR is **not applied to any opcode** today (the helper
  `GameMemory::Object::ToAttackId()` still exists but has no caller). Both 0x11 and 0x19 send the
  **raw** entity index.

## Skill attack 0x19 (TARGETED_SKILL — implemented)
> ⚠️ **This section was rewritten on 2026-11-15.** The older text of this skill (interleaved
> index/skill, `^0xEFF5`, a direction byte, "skill wire 0xE7C3") was a **misread** of the
> capture. Sources: `byp/docs/skill_c1_07_19_fix/` and `byp/docs/stack_c1_07_19/`.

- Plaintext (7 bytes) — **skill first, raw index, no direction byte**:
  ```
  C1 07 19 [skillH] [skillL] [indexH] [indexL]
  ```
  - `skill` = 2-byte skill number = the F3 11 `SkillNumber` shown in the MuProxy2 combo
    (e.g. `0x003C` = 60 = Force do DL).
  - `index` = **raw** entity index (`OFF_INDEX`, 0x07E) — **no `0xEFF5`**.
  - No direction byte: the client sends 1 byte less than the server struct
    (`CGSkillAttackRecv` reads skill + index only).
- Pipeline (**only 0x19** — the 0x11 stays plain C1):
  ```
  C1 07 19 [skillH][skillL][indexH][indexL]
    → XorData [3,7)        → C1 07 19 E7 C3 E2 50   (form seen in the capture)
    → SM input = [serial @0x08793700] + payload XorData'd (6 bytes)
    → SimpleModulus 6→11   → C3 0D [11 bytes]
    → Layer-1               → wire
  ```
- The captured `C1 07 19 E7 C3 E2 50` is the **XorData form, before the SM** — not the wire.
  Decoding it with the XorFilter gives `C1 07 19 00 3C 02 77` (skill 60 = Force, index 0x0277).
  The old "skill wire 0xE7C3" was exactly this misread.
- byp.dll flow: `AttackModule::SendSkillAttack` builds the 7-byte plaintext and calls
  `PacketInjector::Inject(plain, 7)` **directly** — the old queue
  (`EnqueueSkillPacket(SKILL_TYPE_MANUAL)`) was **removed** (Fase 0 of the refactor).
- Log lines to check in the field:
  `SkillAttack: plain=[C1 07 19 00 3C .. ..] -> PacketInjector::Inject`,
  `InjectSkillC1: after XorData [...]`, `InjectSkillC1: sent C3 0D (13 bytes) skill=0x.. index=0x..`.

### Open question — how many bytes go into the SM
- Our implementation feeds **6 bytes** to SimpleModulus (`[serial] + 5 payload bytes`).
- The stack RE of the client's own send (`main.exe+0x4DB0`, see
  `byp/docs/stack_c1_07_19/03_send_encrypt_0x404DB0.md`) shows the client overwrites `data[3]`
  with the **serial** and SM-encrypts only **4 bytes** (`data[3..6]`).
- Not reconciled yet — needs a live breakpoint at `0x404E34` to see what the server really
  expects. Until then, treat our 6-byte variant as *unconfirmed in the field*.
