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
> Paths are post-refactor (`src/` split per domain — see §12). The old flat paths
> (`src/PacketSniffer.cpp`, `src/AttackModule.cpp`, …) no longer exist.

| File | Purpose |
|------|---------|
| `byp/gg_bypass2/src/core/dllmain.cpp` | entry point; installs the hooks synchronously on `DLL_PROCESS_ATTACH` |
| `byp/gg_bypass2/src/core/{Logger,InlineHook,OriginalApi,StackTrace}.cpp` | log, hook helper, ws2_32 trampolines, stack walk |
| `byp/gg_bypass2/src/crypto/MuCipher.cpp` | Layer-1 + `XorData` (the only XOR port) |
| `byp/gg_bypass2/src/crypto/SimpleModulusCipher.cpp` | C3 cipher (SM object layout, Encrypt/Decrypt) |
| `byp/gg_bypass2/src/crypto/C3Decoder.cpp` | C3/C4 payload decoding for the log |
| `byp/gg_bypass2/src/net/winsock/WinsockHooks.cpp` | the ws2_32 hooks + observers |
| `byp/gg_bypass2/src/net/CommandChannel.cpp` | TCP 44412 (commands, entities) |
| `byp/gg_bypass2/src/net/PacketChannel.cpp` | TCP 44413 (PKT stream) |
| `byp/gg_bypass2/src/net/{PacketHandler,MessageRegistry}.cpp` | line dispatch + typed `TAG|payload` registry |
| `byp/gg_bypass2/src/packets/PacketSniffer.cpp` | orchestrator: installs hooks, opens the channels, wires observers |
| `byp/gg_bypass2/src/packets/PacketInjector.cpp` | **single injection door** (0x11 and 0x19 pipelines) |
| `byp/gg_bypass2/src/packets/GameServerConnection.cpp` | GS socket + C116 port detection + transport |
| `byp/gg_bypass2/src/packets/PacketReassembler.cpp` | packet reassembly (callback-based) |
| `byp/gg_bypass2/src/packets/NameRewriter.cpp` | create-char (0x31) encoding — dormant, no call-site |
| `byp/gg_bypass2/src/attack/AttackModule.cpp` | `BuildAttackPacket`, `ComputeDirection`, `SendAttack`, `SendSkillAttack`, Watcher/Dispatcher threads |
| `byp/gg_bypass2/src/debug/{DebugTools,DebugScans}.cpp` | RE tools (dump / hexdump / scanvalue / scanslot / dirscan) |
| `byp/gg_bypass2/src/game/GameMemory/{Globals,Object,Player}.cpp` | object array access, offsets, local player |
| `byp/gg_bypass2/src/game/{MonsterTracker,ViewportMap,SkillMap,SkillHook}.cpp` | tracker, viewport map, F3 skill list, skill-builder hooks |
| `byp/gg_bypass2/src/include/*` | all headers (`MuOpcodes.h`, `Config.h`, `GameAddresses.h`, `GameObject.h`, `CommandParams.h`, …) |
| `byp/gg_bypass2/build.bat` | explicit `SOURCES` list, one line per domain folder (§7) |
| `byp/MuProxy2/MuProxy2/Protocol/Messages.cs` | C# message defs |
| `byp/MuProxy2/MuProxy2/DllCommandService.cs` | 44412 routing |
| `byp/MuProxy2/MuProxy2/TcpLoopbackProxy.cs` | blind relay |
| `byp/docs/attack_packet_re.md` | validated **0x11** attack RE (source of truth) |
| `byp/docs/skill_c1_07_19_fix/` | the **0x19 fix** (2026-11-15): XOR decode + C3 pipeline — current truth |
| `byp/docs/stack_c1_07_19/` | stack RE of the client's own skill send (serial + SM input size) |
| `byp/docs/skill_0x19_diff_analysis.md` | ⚠️ **superseded** — the old "interleaved + 0xEFF5" reading |

## Open questions
- **SM input size on the 0x19 path**: ours is 6 bytes (`[serial]+5 payload`); the client's own
  send RE shows 4 bytes (`data[3..6]`, with the serial overwriting `data[3]`). Needs a live
  breakpoint at `0x404E34` (§4).
- Exact location of Layer-1 in the socket layer (behind `[0xD227F8]`).
- Whether movement packets (`C1 08 D4 02`) use XorData (flag=0 vs flag=1 append).
- Whether the `0xE5` (SKILL_ATTACK, 13-byte) path is ever used by the client, or if `0x19`
  (TARGETED_SKILL) is the only live skill opcode.
