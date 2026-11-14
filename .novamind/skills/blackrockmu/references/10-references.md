# 10. References & meta

## Real packet reference
The **authoritative packet structures** live in the OpenMU docs:
`OpenMU-master/docs/Packets/` — `ClientToServer.md`, `ServerToClient.md`, `PacketTypes.md`,
`ConnectServer.md`, plus per-packet files (e.g. `C4-F3-10-CharacterInventory_by-server.md`).
Consult those for packet layouts/opcodes instead of re-deriving from local notes.

## Emulator source & client version
- **`MuEmu-master/`** is the MU emulator **server-side source** (C++). It ships several client
  versions: `Client_EX401`, `Client_EX603`, `Client_EX803` (and matching `Main_EX401/603/803`).
- **We use version `ex603`.** The relevant folders are `MuEmu-master/Client_EX603/` and
  `MuEmu-master/Main_EX603/`. All hardcoded offsets in `GameAddresses.h` are tied to this ex603
  build — if you switch client version, re-validate every offset.
- Use the emulator source to cross-check server-side packet handling / crypto (e.g.
  `SkillManager.h` `PMSG_SKILL_ATTACK_RECV` for the 0x19 layout) when reverse-engineering the
  client is ambiguous.

## Key files
| File | Purpose |
|------|---------|
| `psdisabled/gg_bypass2/src/PacketSniffer.cpp` | Layer-1 + XorData crypto, `g_XorFilter`, HookSend, `InjectC1Attack`/`InjectToServer`, `EnqueueSkillPacket`, PKT channel |
| `psdisabled/gg_bypass2/src/AttackModule.cpp` | `BuildAttackPacket`, `ComputeDirection`, `ComputeSkillDirection`, `SendAttack`, `SendSkillAttack`, Watcher/Dispatcher threads |
| `psdisabled/gg_bypass2/src/SimpleModulusCipher.cpp` | C3 cipher (SM object layout, Encrypt/Decrypt, NativeEncrypt/Decrypt) |
| `psdisabled/gg_bypass2/src/SkillMap.cpp` | skill-list parser (F3 11 variant 075: 3-byte entries, BigEndian) |
| `psdisabled/gg_bypass2/src/GameMemory.cpp` + `include/GameAddresses.h` + `include/GameObject.h` | object array access + offsets + globals |
| `psdisabled/gg_bypass2/src/include/MuOpcodes.h` | opcodes (0x11 attack, 0x19/0xE5 skill, C1-C4 headers) |
| `psdisabled/gg_bypass2/src/include/Config.h` | feature flags + tunables |
| `psdisabled/gg_bypass2/src/PsBypass.cpp` + `PsPacketHook.cpp` + `MainPatcher.cpp` + `StartupHook.cpp` | PS.dll neutralization + Main.dll patches |
| `psdisabled/MuProxy2/MuProxy2/Protocol/Messages.cs` | C# message defs |
| `psdisabled/MuProxy2/MuProxy2/DllCommandService.cs` | 44412 routing |
| `psdisabled/MuProxy2/MuProxy2/TcpLoopbackProxy.cs` | blind relay |
| `psdisabled/docs/attack_packet_re.md` | validated attack RE (source of truth) |

## Open questions
- Exact location of Layer-1 in the socket layer (behind `[0xD227F8]`).
- Whether movement packets (`C1 08 D4 02`) use XorData (flag=0 vs flag=1 append).
- Whether the `0xE5` (SKILL_ATTACK, 13-byte) path is ever used by the client, or if `0x19`
  (TARGETED_SKILL) is the only live skill opcode.
