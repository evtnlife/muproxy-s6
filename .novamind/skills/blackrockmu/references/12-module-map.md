# 12. Source module map (post-refactor)

> The byp.dll source was reorganized into **domain folders** (change
> `reorganize-byp-modules`). Every file/symbol below lives where this section says —
> older notes that point to `src/PacketSniffer.cpp`, `src/AttackModule.cpp`,
> `src/SimpleModulusCipher.cpp` etc. are **obsolete**.

## Tree (byp/gg_bypass2/src)

```
src/
├── Main.def                     exports da DLL
├── include/                     ALL headers (only include path: /Isrc\include)
│   └── GameMemory/              Globals.h, Object.h, Player.h
├── core/                        dllmain, Logger, InlineHook, OriginalApi, StackTrace
├── crypto/                      MuCipher, SimpleModulusCipher, C3Decoder
├── net/                         CommandChannel, PacketChannel, PacketHandler, MessageRegistry
│   └── winsock/                 WinsockHooks
├── packets/                     PacketSniffer, PacketReassembler, PacketInjector,
│                                GameServerConnection, NameRewriter
├── game/                        MonsterTracker, ViewportMap, SkillMap, SkillHook
│   └── GameMemory/              Globals, Object, Player
├── attack/                      AttackModule
└── debug/                       DebugTools, DebugScans
```

## Hard rules of the layout
1. **No `.cpp` loose in `src/` root.** Every implementation file lives in the folder of its
   domain (`src/<domain>/<Name>.cpp`).
2. **One namespace = one file** (header + implementation). `GameMemory` was split into
   `GameMemory::Globals`, `GameMemory::Object`, `GameMemory::Player` — one header per namespace
   under `include/GameMemory/`.
3. **All headers in `src/include/`** — includes are by bare filename (`#include "MuCipher.h"`),
   never by relative path.
4. **`build.bat` lists every source explicitly** (`SOURCES`, one line per folder). A new `.cpp`
   that is not added there **is not linked** — no globbing, no folder scanning.
5. Modules own their messages: `REGISTER_MESSAGE(TAG, Handler)` lives in the module's own `.cpp`
   (`MessageRegistry.h` has the macro).
6. Shared command parsing goes through `CommandParams` (`include/CommandParams.h`):
   `Find` / `ParseIntParam` / `ParseByteParam` / `ParseFloatParam` / `ParseHexParam`.

## Who owns what

| Module | File | Responsibility |
|--------|------|----------------|
| `DllMain` | `core/dllmain.cpp` | Entry point. Hooks are installed **synchronously** in `DLL_PROCESS_ATTACH` (order matters — see §1). |
| `Logger` | `core/Logger.cpp` | The single log file (`gg_bypass2.log`). |
| `InlineHook` | `core/InlineHook.cpp` | Generic inline-hook helper (5-byte JMP patch). |
| `OriginalApi` | `core/OriginalApi.cpp` | Holds the ws2_32 trampolines (`send`, `connect`) installed by the hook module. Internal channels use them so their own traffic never passes through the hook. |
| `StackTrace` | `core/StackTrace.cpp` | EBP walk used by the packet log (`SEND stack:`). |
| `MuCipher` | `crypto/MuCipher.cpp` | Layer-1 (XOR+sub) and Layer-2 (`XorData`). **The only XOR port of the project.** |
| `SimpleModulusCipher` | `crypto/SimpleModulusCipher.cpp` | SM (modulus) cipher — C3 path only. |
| `C3Decoder` | `crypto/C3Decoder.cpp` | Decodes/logs C3-C4 payloads (`SEND C3 decrypt:` …). |
| `CommandChannel` | `net/CommandChannel.cpp` | TCP **44412** to MuProxy2, line-based text (commands, entities). Was `TcpClient`. |
| `PacketChannel` | `net/PacketChannel.cpp` | TCP **44413** to MuProxy2, `PKT\|SEND\|<hex>` stream. |
| `PacketHandler` | `net/PacketHandler.cpp` | Parses inbound lines, dispatches to `MessageRegistry`. |
| `MessageRegistry` | `net/MessageRegistry.cpp` | Typed `TAG\|payload` registry + `REGISTER_MESSAGE` macro. |
| `WinsockHooks` | `net/winsock/WinsockHooks.cpp` | The ws2_32 hooks (send/recv/WSASend/WSARecv/connect) + observers. |
| `PacketSniffer` | `packets/PacketSniffer.cpp` | **Orchestrator only** (~180 lines): installs hooks, opens the channels, wires the observers. |
| `PacketReassembler` | `packets/PacketReassembler.cpp` | Packet reassembly, reports through a callback. |
| `PacketInjector` | `packets/PacketInjector.cpp` | **The single injection port** (§4). |
| `GameServerConnection` | `packets/GameServerConnection.cpp` | Owns the GS socket + port detection (C116) and is the **transport** (`SendToGameServer`). |
| `NameRewriter` | `packets/NameRewriter.cpp` | create-char (0x31) name/class encoding — **dormant**: no call-site today, kept as RE tool. |
| `MonsterTracker` | `game/MonsterTracker.cpp` | Scans the object array, reports `ENTITY` on 44412. |
| `ViewportMap` / `SkillMap` / `SkillHook` | `game/*.cpp` | Viewport map, F3 skill list parser, skill-builder hooks. |
| `GameMemory::Globals/Object/Player` | `game/GameMemory/*.cpp` | main.exe/Main.dll bases, object array access, local player. |
| `AttackModule` | `attack/AttackModule.cpp` | Auto-attack loop, `BuildAttackPacket`, `SendAttack`, `SendSkillAttack`. |
| `DebugTools` / `DebugScans` | `debug/DebugTools.cpp`, `debug/DebugScans.cpp` | RE tools (dump/hexdump/scan). Log tag stays `ATTACK` so log lines are unchanged. |

## Data flow of an injected packet

```
AttackModule::SendAttack / SendSkillAttack
        │  plaintext real: C1 07 11 …  /  C1 07 19 …
        ▼
PacketInjector::Inject(plaintext, len)          ← porta ÚNICA
        │  MuCipher::XorData(data, 3, len)        ← Camada-2, aplicada 1× só
        │  opcode 0x11 → Layer-1
        │  opcode 0x19 → SM(serial+payload) → C3 0D [11] → Layer-1
        ▼
GameServerConnection::SendToGameServer(cipher)   ← dono do socket do GS
        ▼
ws2_32 send (trampoline original)
```

`PacketInjector` **never touches the socket** and `GameServerConnection` **never ciphers** —
the two responsibilities are separate on purpose.
