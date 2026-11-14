---
name: blackrockmu
description: BlackRockMU private-server client knowledge — byp.dll bypass (C++), MuProxy2 control UI (C#), packet crypto (Layer-1/XorData/skill), attack/skill injection, object/monster tracking, and build/deploy. Load this skill for ANY work on the F:\blackrockmu project: editing gg_bypass2/ or MuProxy2/ code, debugging corrupted or rejected attack packets, adding a new message tag, reverse-engineering packet crypto, looking up object-array offsets, or rebuilding/deploying after a main.exe change. Use it even when the user does not name the skill or the files.
---

# BlackRockMU — Project Knowledge

> Use this skill for ANY work on the BlackRockMU private-server client at `F:\blackrockmu`:
> the `byp.dll` bypass (C++), the `MuProxy2` control UI (C#), packet crypto, attack/skill
> injection, or object/monster tracking. Load it before editing code in `gg_bypass2/` or `MuProxy2/`.
>
> **Golden rule:** the one who decrypts is `byp.dll`. `MuProxy2` (C#) **never** touches crypto.

Each topic below is a one-line summary; the full detail (offsets, crypto, validated packets,
debugging) lives in the referenced file. **Read the relevant reference before editing.**

| # | Topic | One-liner | Reference |
|---|-------|-----------|-----------|
| 1 | Architecture & attach flow | main.exe → opengl32.dll (GLProxy) → byp.dll; ws2_32 hooks; MuProxy2 UI. | `references/01-architecture.md` |
| 2 | Communication channels | 44412 CMD, 44413 PKT, 44411/55902 blind relay; the `SEND` command. | `references/02-channels.md` |
| 3 | Crypto | Layer-1 (XOR 0x36/sub 0x07), XorData (Camada-2), C3 SimpleModulus. | `references/03-crypto.md` |
| 4 | Attack packets | 0x11 normal (raw index, XorData+Layer-1); 0x19 skill (interleaved, ^0xEFF5, C3). | `references/04-attack-packet.md` |
| 5 | Object array & offsets | object-array chain, GameObject offsets, globals, skill-builder RVAs. | `references/05-object-array.md` |
| 6 | Message protocol (44412) | `TAG\|payload`, `[MessageTag]`, how to add a message. | `references/06-message-protocol.md` |
| 7 | Build & deploy | build.bat / deploy.bat / dotnet build → F:\blackrockmu\. | `references/07-build-deploy.md` |
| 8 | Feature flags & config | Config.h flags + tunables. | `references/08-config.md` |
| 9 | Golden rules & debugging | anti-gambiarra rules + failure-mode table. | `references/09-golden-rules-debug.md` |
| 10 | References & meta | OpenMU packet docs, ex603 emulator, key files, open questions. | `references/10-references.md` |

## Quick facts (most-used)
- **Inject a normal attack:** build TRUE plaintext `C1 07 11 <idxHi> <idxLo> 78 <dir 0..7>`, call
  `InjectC1Attack` (does XorData[3,len) + Layer-1). Never feed XorData-form bytes to it (double-encrypt).
- **Inject raw hex from the UI:** `HandleSendPacket` → `InjectToServer` (Layer-1 only; bytes must
  already be XorData form).
- **Skill 0x19:** `C1 07 19 [idxH][skillH][idxL][skillL]`, target = `entityIndex ^ 0xEFF5`, then
  C3 (SimpleModulus). Direction is 0..255 (not 0..7).
- **Object array:** `*(main.exe+0xA7C224)` → `+0x8` → array; `object[i] = array + i*0x598`
  (400 slots). Slot index is NOT derivable from entityIndex.
- **All offsets are ex603-specific** — re-validate if `main.exe` changes.
