# 11. Log analysis (gg_bypass2.log)

> The log is the primary debug tool for byp.dll. Read this before interpreting a debug session —
> it documents the exact line formats, what is intentionally absent, and how to diagnose failures.

## Where the log lives
- **Single file:** `F:\blackrockmu\gg_bypass2.log` (game dir). Opened append-mode with
  `_SH_DENYNO`, so MuProxy2 can read it concurrently while byp.dll writes.
  `byp_packets.log` **no longer exists** — system and packet logs were consolidated into the one file.
- MuProxy2's own C# logs go to `F:\blackrockmu\logs\` (subfolder) — different thing, no conflict.
- Line format (`Logger::Log`): `[YYYY-MM-DD HH:MM:SS] [TAG] message`.
- Tags in use: `PACKETSNIFFER` (hooks + injection), `ATTACK` (attack module **and** the debug
  tools), `POTION` (auto-potion), `SMOD` (SimpleModulus cipher), `PACKETHANDLER` (inbound line
  dispatch), `MONSTER` (tracker), `GAMEMEMORY` (bases/globals), `VIEWPORT`, `SKILLMAP`,
  `SKILLHOOK`, `SMODHOOK`.
- `Logger::LogPacket` / `LogPacketDual` (the old `hex=... dec=...` formats) are currently
  **dead code** — no callers. Do not expect `hex=/dec=` lines in the log.

## What is NOT in the log (by design — don't look for it)
- **No raw hex dump from `LogRaw`.** `LogRaw` only reassembles + forwards to the proxy.
  The detailed per-packet log (stack trace, C3/C4 decrypt, SMOD blocks) happens in
  `HookSend` / `HookWSASend`.
- **No `NativeDecrypt` lines.** That function was removed (Obj1 DecKey is zeros, so it always
  failed C→S). C3/C4 decryption uses byp.dll's own `Decrypt` (DecKey = modular inverse of EncKey).
- **No per-packet log for normal (0x11) attacks.** Only skill (0x19/C3) attacks and lifecycle
  events are logged. Silence on normal attacks is expected, not a bug.
- **No `CONNECT to 127.0.0.1:44412` spam.** The internal channels connect through the original
  ws2_32 API (`OriginalApi`), so the hook never sees them (§2).

## A healthy C3 (skill attack) SEND looks like this
```
[PACKETSNIFFER] SEND C3 24 bytes [C3 18 C6 DC ...]
[PACKETSNIFFER] SEND stack: Main.dll+0xBB3DE ...
[SMOD] SM input bytes (22): [C6 DC 37 B7 ...]
[SMOD] DecryptBlock #27: src=[C6 DC 37 B7 ...]
[SMOD]   ring=[XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX] finale=[XX XX]
[SMOD]   checksum calc=0xXX expected=0xXX numBytes=N
[PACKETSNIFFER] SEND C3 decrypt: OK decLen=11
[PACKETSNIFFER] SEND C3>C1 decoded: [C1 0C 0E ...] dk=0A
```
- `SEND stack:` — EBP-walk of the caller chain (who sent the packet).
- `SM input bytes` — the exact bytes handed to the SM decrypt (wire minus the 2-byte header).
- `dk=` — the decrypt-key byte (first byte of the decrypted SM output).
- C4 packets: same shape, `SEND C4>C2 decoded:`.

## Injection lines (`PACKETSNIFFER` tag, from `packets/PacketInjector.cpp`)
The prefix **`InjectSkillC1:`** was kept on purpose — it is the old function name, preserved so
the field-verification recipe (`InjectSkillC1: after XorData [...]`) keeps working.
```
[ATTACK]        SkillAttack: plain=[C1 07 19 00 3C 02 77] -> PacketInjector::Inject
[PACKETSNIFFER] InjectSkillC1: after XorData [C1 07 19 E7 C3 E2 50]
[PACKETSNIFFER] InjectSkillC1: SM input [.. .. .. .. .. ..] (serial consumido)
[PACKETSNIFFER] InjectSkillC1: SM encrypted [.. .. ..] len=11
[PACKETSNIFFER] InjectSkillC1: sent C3 0D (13 bytes) skill=0x003C index=0x0277
```
- The 0x11 path logs **nothing** per packet (intentional).
- Failure lines: `InjectSkillC1: SM encrypt failed (smLen=..)`, `InjectSkillC1: send failed`,
  `Inject: sender nao registrado`.
- Transport failures (from `GameServerConnection::SendToGameServer`, **the text still says
  `InjectToServer:`** even though the function was renamed):
  `InjectToServer: GS socket nao rastreado`, `InjectToServer: socket invalido (err=..), limpando`,
  `InjectToServer: send error N - socket morto, limpando`.

## SMOD per-block logging (`DecryptBlock`)
- **Always (every block):** `DecryptBlock #N: src=[11B]`, `ring=[4x DWORD] finale=[2B]`,
  `checksum calc=0x.. expected=0x.. numBytes=..`.
- **Verbose (first 10 blocks only, `dbgCount < 10`):** keys (`Mod/Dec/Xor`), `ringBuf[0..3]`,
  `ringAfterXOR`, per-slot `slot[i]: ring=.. * dec=.. mod mod=.. = .. (32bit=.. product64=..)`,
  `plaintext=[8B]`.
- **On checksum failure:** `DecryptBlock: checksum FAILED` + live-vs-cached key comparison
  (`SM object prefix`, `Live Mod=[..] Cached=[..]`, `Live Enc=[..] Cached=[..]`) — the keys
  changed in memory (re-login/re-key). Then `Decrypt: block N/M checksum FAILED`.
- The `#N` counter is a per-process `static int dbgCount` — it does NOT reset on re-init.

## SMOD init block (first init only, `g_initCount == 1`)
1. `SM object at 0x.., prefix=0x00D546EC, first 72 bytes:` + 5 rows of 16 bytes.
2. `Keys loaded (#1):` — `Mod/Enc/Dec(stored)/Xor`. `stored=` is the DecKey at +0x24
   (zeros for the C2S Obj1 — expected).
3. `Verify mod inverse:` — per slot `(enc * dec) % mod = 1 OK|FAIL`. **Any FAIL = keys are wrong.**
4. `Roundtrip: encrypted(N)=[..]` then `decrypted(N)=[..] MATCH|MISMATCH` — byp.dll encrypts
   8 test bytes and decrypts them back. **MISMATCH = cipher implementation is broken.**
5. `NativeEncrypt->ourDecrypt:` — calls the game's encrypt at `0x998420`, decrypts with ours.
   **MISMATCH = our Decrypt diverges from the game's Encrypt.**
- On re-login the keys change: `Keys loaded (#2):` etc. appear, but the self-test runs only once.

## ATTACK tag
- **Skill attack (0x19):** `ComputeSkillDir: entity=.. p(..) t(..) angleDeg=.. dirByte=0x..`,
  `SkillAttack: entityIndex=.. compositeIdx=.. objId=.. slotIndex=.. skill=0x..`,
  `SkillAttack: entity=.. DIRECT=.. XOR=.. gameGlobal=.. dir=.. skill=..`,
  `SkillAttack: plain=[..] -> PacketInjector::Inject`.
- **Normal attack (0x11):** no per-packet log (intentional).
- **Lifecycle:** `Watcher iniciado/encerrado`, `Dispatcher iniciado (delay=.., range=.., hitcount=..)`,
  `Watcher: novo monstro 0x.. detectado` / `removido`.
- **On-demand diagnostics** (the debug tools, `src/debug/` — still tagged `ATTACK`):
  `==== DUMP FULL ObjectManager local=(..,..) ====`, `==== FIM DUMP N objs vivos ====`,
  `==== HEXDUMP slot=.. type=.. live=.. idx=.. name='..' ====`,
  `==== SCANVALUE 0x.. ====` / `==== FIM SCANVALUE ====`,
  `==== SCANSLOT slot=.. … search=0x.. ====` / `==== FIM SCANSLOT ====`,
  `==== DIRSCAN player=0x.. idx=0x.. slot=.. name='..' ====`.

## POTION tag (auto-potion — see §13)
```
[POTION] uso de item observado (cliente): slot=25 target=0xFF type=0x00
[POTION] modelo de HP aprendido (slot=25)
[POTION] potion de HP disparada em 38% (slot=25) — uso injetado
[POTION] estado: LIGADO | hp<50% mp<30% intervalo=1000ms | modelo HP=ok (slot 25) | ...
```
- `uso de item observado` = o **jogador** bebeu a potion (é assim que o módulo
  descobre o slot do inventário);
- `modelo de HP/MP aprendido` = o efeito (life/mana subindo) confirmou o tipo;
- `potion de ... disparada` = o auto-potion injetou o uso (o `PacketInjector` também
  loga `Inject: uso de item C1 26 [...]` na tag `PACKETSNIFFER`).

## Other one-shot / event lines
- `Chaves: xor=0x36 sub=0x07 (mem: xor=.. sub=..)` — Layer-1 keys, logged once. If the
  `mem:` values differ from the defaults, the client changed them.
- `C116: porta do GS detectada = N` — GS port auto-detection from the C116 (ConnectToServer) packet.
- `Game server socket rastreado via SEND: N (porta P)` — GS socket captured via the fallback path.
- `Conectado ao MuProxy2 via TCP 127.0.0.1:44412` — control channel up.
- `PACKETHANDLER RECV linha (N): [..]` — every inbound line on 44412 (useful to confirm a
  command actually reached byp.dll).
- `GAMEMEMORY main.exe base = 0x..` / `Main.dll base = 0x..` — module bases.
- `MONSTER Lista enviada: N objetos (filter=0x.., range=..)` — tracker batch.

## Debug recipes
| You see | Meaning / next step |
|---------|---------------------|
| `SEND C3 decrypt: FAILED (decLen=..)` | SM decrypt failed. Look at the `checksum calc/expected` line just above. |
| `checksum calc=.. expected=..` (differ) | Key mismatch or wrong input bytes. If followed by `Live .. Cached ..` diff → keys changed (re-login); needs re-init. |
| `Verify mod inverse: FAIL` | Enc/Dec key pair inconsistent — bad read of the SM object (wrong offset / pre-login). |
| `Roundtrip: MISMATCH` | byp.dll cipher bug (encrypt/decrypt asymmetry). |
| `NativeEncrypt->ourDecrypt: MISMATCH` | Our Decrypt diverges from the game's Encrypt — compare the `slot[i]` 32bit vs product64 lines. |
| `Initialize: modulus all zero` | Keys not ready (pre-login). Expected; it retries. |
| No `SEND` lines at all | Hook not firing or wrong socket/port. Check GS port detection and the `Game server socket rastreado` line. |
| `Falha ao conectar 127.0.0.1:44412` | MuProxy2 not listening; byp.dll retries every 1s (log throttled to 10s). |
| `Inject: sender nao registrado` | The hooks module did not register the transport — `PacketSniffer::Install()` did not run (or hooks failed to install). |
| `InjectToServer: GS socket nao rastreado` | The game socket to the GS was never captured — check the C116 port detection and the `SEND` fallback path. |
| `PACKETHANDLER RECV linha` shows the command but nothing happens | The tag is not registered in the C++ `MessageRegistry` (see §6). |
