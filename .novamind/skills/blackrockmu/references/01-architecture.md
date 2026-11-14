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
  authenticated TCP socket** to the GS (`g_GameServerSocket` in `PacketSniffer.cpp`). That socket
  is already connected to `MuProxy2 → relay → real GS`. `InjectC1Attack` / `InjectToServer` write
  straight onto it via `TrueSend`, so the server sees the packet from the same authenticated session.

## byp.dll modules
- **PacketSniffer**: ws2_32 hooks, Layer-1 + XorData crypto, injection, PKT channel (44413).
- **AttackModule**: auto-attack loop (Watcher / Dispatcher / AttackSingleMob threads),
  `BuildAttackPacket`, `SendSkillAttack`.
- **MonsterTracker**: scans the object array, reports `ENTITY` via 44412.
- **GameMemory / GameAddresses / GameObject**: object-array access + offsets.
- **SimpleModulusCipher**: C3 cipher (skill attacks).
- **SkillMap**: parses the F3 skill list.
- **PsBypass / PsPacketHook / MainPatcher / StartupHook / DllNotifyHook / VPTracer**:
  PS.dll (Themida/WinLicense) neutralization + Main.dll patches.
