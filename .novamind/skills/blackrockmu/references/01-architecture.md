# 1. Architecture & attach flow

## Load chain
```
main.exe (MU Online x86, ex603)
  └─ loads opengl32.dll from the game dir (F:\blackrockmu\)
       └─ opengl32.dll = GLProxy wrapper (gg_bypass2/opengl/opengl_proxy.cpp)
            └─ DllMain does LoadLibraryA("byp.dll") (opengl_proxy.cpp:~423)
               then forwards all GL calls to the real System32 opengl32.dll
                 └─ byp.dll hooks ws2_32 (send/recv/WSASend/WSARecv/connect)
                    and talks to MuProxy2 over TCP localhost
                         ├─ TCP 44412 (CMD): commands, entities, logs
                         └─ TCP 44413 (PKT): decrypted packets (PacketSniffer)

MuProxy2.exe (C# .NET 8 WinForms)
  ├─ HookListenerService : listens 44412 + 44413
  ├─ DllCommandService   : routes messages (MessageRegistry)
  ├─ TcpLoopbackProxy    : blind MITM relay 44411/55902 (never inspects)
  ├─ MonsterService      : tracks monsters / player position
  ├─ AttackService       : controls auto-attack
  └─ UI: Packet List, Monster List, Logs, attack controls
```

## How byp.dll gets in
- **NOT injected via `CreateRemoteThread`.** The game loads `opengl32.dll` naturally; that
  DLL's `DllMain` loads `byp.dll`.
- The old `MuProxy2.Hook.dll` named-pipe (`\\.\pipe\MuProxy2`) + `CreateRemoteThread` path is
  **obsolete** — do not use it.
- `opengl32.dll` in the game dir MUST be the GLProxy wrapper (not System32's), and `byp.dll`
  must sit next to it — otherwise byp.dll never loads.

## Injection socket (key detail)
- byp.dll does **not** open its own connection to the game server. It reuses the **game's own
  authenticated TCP socket** to the GS (`GameServerConnection` owns it —
  `packets/GameServerConnection.cpp`). That socket is already connected to
  `MuProxy2 → relay → real GS`. `PacketInjector` ciphers the bytes and
  `GameServerConnection::SendToGameServer` writes them straight onto it via the
  **original** `send` trampoline (`OriginalApi::GetSend()`), so the server sees the packet from
  the same authenticated session.

## byp.dll modules (post-refactor — see §12 for the full map)
The source is split per domain under `src/` (`core/`, `crypto/`, `net/`, `packets/`, `game/`,
`attack/`, `debug/`); headers all live in `src/include/`.

- **`packets/PacketSniffer`**: orchestrator only — installs the hooks, opens the channels, wires
  the observers.
- **`net/winsock/WinsockHooks`**: the ws2_32 hooks (send/recv/WSASend/WSARecv/connect) + the
  observer callbacks the sniffer consumes.
- **`crypto/MuCipher`**: Layer-1 (XOR+sub) and Layer-2 (`XorData`) — the only XOR port.
- **`crypto/SimpleModulusCipher`**: SM cipher (skill attacks). **`crypto/C3Decoder`**: C3/C4 log decoding.
- **`packets/PacketInjector`**: the single injection port (§4).
- **`packets/GameServerConnection`**: owns the GS socket + port detection (C116) and is the transport.
- **`packets/PacketReassembler`** (reassembly via callback), **`packets/NameRewriter`**
  (create-char encoding — **dormant**, no call-site).
- **`net/CommandChannel`** (44412), **`net/PacketChannel`** (44413), **`net/MessageRegistry`**
  (typed `TAG|payload` registry), **`net/PacketHandler`**.
- **`attack/AttackModule`**: auto-attack loop (Watcher / Dispatcher threads),
  `BuildAttackPacket`, `SendAttack`, `SendSkillAttack`.
- **`debug/DebugTools` + `debug/DebugScans`**: RE tools (dump/hexdump/scan) — log tag stays `ATTACK`.
- **`game/MonsterTracker`**: scans the object array, reports `ENTITY` via 44412.
- **`game/GameMemory/{Globals,Object,Player}`**: object-array access + offsets + local player.
- **`game/{ViewportMap,SkillMap,SkillHook}`**: viewport map, F3 skill list, skill-builder hooks.
- **`core/{dllmain,Logger,InlineHook,OriginalApi,StackTrace}`**: entry point, log, hook helper,
  ws2_32 trampolines, stack-trace walker.

> The old `PsBypass / PsPacketHook / MainPatcher / StartupHook / DllNotifyHook / VPTracer`
> modules **no longer exist** in the source tree — do not look for them.
