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
| `SendAttack` → `BuildAttackPacket` | TRUE plaintext `C1 07 11 00 1A 78 04` | `InjectC1Attack` | XorData `[3,len)` **then** Layer-1 |
| `HandleSendPacket` (user hex) | XorData form `C1 07 11 ED 1F 44 86` | `InjectToServer` | Layer-1 only |

- `InjectC1Attack` (PacketSniffer.cpp) validates header `C1` + opcode `0x11`, applies
  `GameServerXorData(3, len)`, then delegates to `InjectToServer` (Layer-1 + `TrueSend` on the
  game's GS socket).
- The `0xEFF5` XOR convention belongs to **skill 0x19**, NOT normal attack 0x11.

## Skill attack 0x19 (TARGETED_SKILL — implemented)
- Server format (GAMESERVER_UPDATE>=701, `SkillManager.h` `PMSG_SKILL_ATTACK_RECV`):
  ```
  C1 07 19 [indexH] [skillH] [indexL] [skillL]
  ```
  - Bytes are **interleaved**: index and skill alternate H/L.
  - Server reads: `bIndex = MAKE_NUMBERW(indexH,indexL)`, `skill = MAKE_NUMBERW(skillH,skillL)`.
  - `index` (target) = **entityIndex ^ 0xEFF5** (the value the game stores at global `0x081C03A4`).
  - `skill` = full 2-byte skill number (e.g. `0x01E7` = Inferno).
- The MuProxy2 UI "Atk ID" column = `entityIndex ^ 0xEFF5` (e.g. a monster shown as `EBEB` is
  entity `0xEBEB ^ 0xEFF5 = 0x0218`). So the packet must send the **XOR'd** value (`0xEBEB`), not
  the raw entity (`0x0218`).
- Skill direction is **0..255** (different from normal attack 0..7):
  `direction = ROUND(angleDeg / 360 * 256) & 0xFF`.
- byp.dll flow: `AttackModule::SendSkillAttack` builds the 7-byte `C1 0x19` plaintext →
  `EnqueueSkillPacket(SKILL_TYPE_MANUAL)` → `HookSend` on the game thread SM-encrypts it (8→11) →
  wraps `C3` (`C3 0D <11 bytes>`) → `InjectToServer` (Layer-1 + send).
- Evidence (real game packet): `C1 07 19 E7 C3 E2 CF` → skill=`0xE7C3`, target=`0xE2CF`
  (`0xE2CF ^ 0xEFF5 = 0x0D3A` entity).
