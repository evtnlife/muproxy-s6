# NovaMind Consolidated Knowledge

> **Reset em 2026-11-15** — logs de sessão antigos removidos para iniciar trabalho novo.
> O conhecimento durável do projeto BlackRockMU já vive em dois lugares; este arquivo
> volta a ser apenas o log de consolidação das próximas sessões.

## Onde está o contexto durável

- **Skill `blackrockmu`** (fonte de verdade) — `F:\blackrockmu\.novamind\skills\blackrockmu\SKILL.md`
  (índice conciso) + `references/01..10-*.md` (arquitetura, canais, criptografia, pacote de
  ataque, array de objetos, protocolo de mensagens, build/deploy, config, regras/debug, referências).
  **Carregar antes de editar `byp/gg_bypass2/` ou `byp/MuProxy2/`.**
- **Memórias persistentes** — fluxo de attach do MuProxy2 (byp.dll via DllMain do opengl32.dll,
  TCP 44412 CMD / 44413 PKT), skill como fonte de verdade, e versão do cliente **ex603**
  (offsets de `GameAddresses.h` amarrados a ela — revalidar se a versão mudar).
- **Docs de referência** — `F:\blackrockmu\byp\docs\` (inclui `attack_packet_re.md`, source of truth
  do RE de ataque). Pacotes reais: `OpenMU-master/docs/Packets/`.

## Regras rápidas
- Pasta do projeto: `F:\blackrockmu\byp\` (renomeada de `psdisabled`).
- Build: `byp/gg_bypass2/build.bat` (byp.dll) e `byp/gg_bypass2/opengl/build.bat` (opengl32.dll);
  MuProxy2 via `dotnet build`.
- Usuário se comunica em português (BR).

---

## Knowledge Consolidation - 2026-11-15T01:40:57.982Z

## Summary

### Original Task
1. **Consolidate logs**: Only ONE log file in the game folder (`gg_bypass2.log`). Remove `byp_packets.log` and any other scattered logs.
2. **Reduce attack logs**: Only log skill attacks. Normal attacks work fine and don't need logging.
3. **Improve packet sniffer format**: Replace noisy raw hex logs with a clean `dec=<decoded> enc=<encoded wire>` format for both RECV and SEND directions. For C3/C4 packets, show the fully SM-decrypted content.

### Files Modified
- `f:\blackrockmu\byp\gg_bypass2\src\include\Config.h` — Removed `PACKET_LOG_FILENAME` constant
- `f:\blackrockmu\byp\gg_bypass2\src\include\Logger.h` — `Initialize` now takes one path; updated `LogPacketDual` comment to reflect `dec=... enc=...` format
- `f:\blackrockmu\byp\gg_bypass2\src\Logger.cpp` — Removed `g_PacketLogFile`; `LogPacket` uses `enc=` label; `LogPacketDual` outputs `dec=<decoded> enc=<wire>`; both write to single `g_LogFile`
- `f:\blackrockmu\byp\gg_bypass2\src\dllmain.cpp` — `InitializeLogging` builds single path only
- `f:\blackrockmu\byp\gg_bypass2\src\AttackModule.cpp` — Removed 4 normal-attack log sites: `ComputeDirection`, `SendAttack` (per-hit), `SendConfiguredAttack` (redundant diagnostic), `Dispatcher` per-cycle. Also removed unused `obj` variable. Kept: skill attack logs, lifecycle events (Watcher/Dispatcher start/stop, monster add/remove), on-demand diagnostics (DUMP/HEXDUMP/SCANVALUE/SCANSLOT/DIRSCAN)
- `f:\blackrockmu\byp\gg_bypass2\src\PacketSniffer.cpp` — Major cleanup:
  - Removed `LogStackTrace` function entirely (was lines 17–55)
  - Added `TryDecodeC3C4` helper (SM-decrypts C3/C4 packets for display)
  - `LogRaw` now uses `TryDecodeC3C4` for C3/C4, calls `LogPacketDual` with decoded+encoded
  - `HookSend`: removed EBP capture, display decryption, `SEND %s %d bytes` log, stack trace, entire C3/C4 SM debug block
  - `HookRecv`: removed `recv() ret=` log
  - `HookWSASend`: removed hex dump + log + stack trace
  - `WSARecv`: removed diagnostic log
  - `ProcessSkillQueue` BUILDER path: removed step-by-step logs (addData args, chained done, after addData dump, encrypt&send done, destroy done)
  - `ProcessSkillQueue` MANUAL path: removed step-by-step logs (C1 payload, SM encrypted, C3 packet, sent C3 packet)

### Files Read (for context)
- `f:\blackrockmu\byp\gg_bypass2\src\include\SimpleModulusCipher.h` — SM cipher API (`NativeDecrypt`, `SM_ENCRYPTED_SIZE`)
- `f:\blackrockmu\byp\gg_bypass2\build.bat` — Build+deploy script (compiles byp.dll, kills main.exe, copies DLL)
- `f:\blackrockmu\byp\MuProxy2\MuProxy2\MainForm.cs` — MuProxy2 writes to `logs/` subfolder (not game root), no conflict
- `f:\blackrockmu\byp\gg_bypass2\src\VPTracer.cpp` — Writes `vptrace.log` but NOT in current build (no file exists)

### Key Decisions
- **Single log file**: All output (general + packets) goes to `gg_bypass2.log` in game folder
- **Packet format**: `dec=<decoded hex> enc=<wire hex>` — decoded first, encoded second
- **C3/C4 full decryption**: `TryDecodeC3C4` performs Layer-1 XOR+sub AND SM decryption to show the true plaintext
- **Injected packets**: `InjectToServer` uses `TrueSend` (bypasses HookSend), so needs its own explicit `LogPacketDual` call — this is the remaining edit
- **Attack logs**: Only skill attacks logged (ComputeSkillDir, SkillAttack entries). Normal attacks produce zero log output.

### Errors
- `list_directory "gg_bypass2"` failed — correct path is `byp/gg_bypass2` (subfolder under `byp/`)
- No compilation errors; Phase 1 build succeeded and deployed `byp.dll` (68608 bytes)

### Current State
- **Phase 1 (log consolidation + attack log reduction)**: ✅ COMPLETE, built, deployed
- **Phase 2 (packet sniffer format)**: 🔄 IN PROGRESS
  - ✅ Logger.cpp/h format changes done
  - ✅ PacketSniffer.cpp noise removal done (LogStackTrace, HookSend/Recv/WSASend/WSARecv, ProcessSkillQueue)
  - ✅ TryDecodeC3C4 helper added, LogRaw updated
  - ⬜ **Remaining**: Add `LogPacketDual` call in `InjectToServer` (was reading lines 1395–1425 to get exact text for the edit)
  - ⬜ **Remaining**: Build with `build.bat` and verify
  - ⬜ **Remaining**: Deploy and confirm single log file works

### Next Steps
1. Add unified `LogPacketDual("PACKETSNIFFER", "SEND", decrypted, decLen, encrypted, encLen)` call in `InjectToServer` after encryption, before `TrueSend`
2. Run `cmd /c byp\gg_bypass2\build.bat` to compile and deploy
3. Verify build succeeds with no errors
---

## Knowledge Consolidation - 2026-11-15T03:14:41.052Z

## Summary

### Original Task
1. **Consolidate logs**: Only ONE log file (`gg_bypass2.log`). Remove `byp_packets.log`.
2. **Reduce attack logs**: Only log skill attacks; normal attacks produce zero log output.
3. **Packet sniffer format**: After multiple iterations, the user clarified they want the **old detailed format** (stack trace + human-readable C3/C4 decryption + SMOD logs) but **without** the "polluting" giant hex dump from `LogRaw`.

### Files Modified (all under `f:\blackrockmu\byp\gg_bypass2\src\`)
- **`include/Config.h`** — Removed `PACKET_LOG_FILENAME` constant
- **`include/Logger.h`** — `Initialize` takes one path; comment reflects `hex=<raw> dec=<decrypted>` format
- **`Logger.cpp`** — Single `g_LogFile`; `LogPacket` uses `hex=` label; `LogPacketDual` outputs `hex=<raw> dec=<decrypted>` (raw first, old order)
- **`dllmain.cpp`** — `InitializeLogging` builds single path only
- **`AttackModule.cpp`** — Removed 4 normal-attack log sites (`ComputeDirection`, `SendAttack` per-hit, `SendConfiguredAttack` diagnostic, `Dispatcher` per-cycle). Kept: skill attack logs, lifecycle events, on-demand diagnostics
- **`PacketSniffer.cpp`** — Reverted to HEAD (restored `LogStackTrace`, detailed `HookSend`/`HookWSASend` logs, `ProcessSkillQueue` step-by-step). Then:
  - Removed `LogPacketDual`/`LogPacket` calls from `LogRaw` (the "polluting" hex dump); kept reassembly + `FlushReasmBuffer`
  - Replaced native+fallback decrypt with **direct own `Decrypt` call** (removed `NativeDecrypt` attempt)
  - Updated comments to document why native was removed
- **`SimpleModulusCipher.cpp`** — Removed `NativeDecrypt` function (dead code); added comment noting removal reason
- **`include/SimpleModulusCipher.h`** — Removed `NativeDecrypt` declaration

### Files Read (for context)
- `f:\blackrockmu\byp\gg_bypass2\src\include\SimpleModulusCipher.h` — SM cipher API
- `f:\blackrockmu\byp\gg_bypass2\build.bat` — Build+deploy script
- `f:\blackrockmu\byp\MuProxy2\MuProxy2\MainForm.cs` — MuProxy2 writes to `logs/` subfolder, no conflict
- `f:\blackrockmu\byp\gg_bypass2\src\VPTracer.cpp` — Writes `vptrace.log` but not in current build
- `f:\blackrockmu\byp\gg_bypass2\src\SimpleModulusCipher.cpp` — Full SM cipher implementation (599 lines)

### Key Decisions
- **Single log file**: All output → `gg_bypass2.log` in game folder
- **Packet format**: `hex=<raw> dec=<decrypted>` — raw wire bytes first, then decrypted
- **C3/C4 decryption**: Only own `Decrypt` (computes DecKey via `ModInverse32(encKey, modulus)`). Native removed because game's `SimpleModulus::Decrypt` at `0x998490` uses stored DecKey at `+0x24` which is **zero** for C2S object (`0x8797778`)
- **LogRaw**: No longer logs any hex; only does reassembly + proxy forwarding
- **HookSend/HookWSASend**: Keep full detailed logs — `SEND %s %d bytes [%s]`, `SEND stack:` (EBP walk), `SEND C3 decrypt: OK/FAILED`, `SEND C3>C1 decoded: [...] dk=...`, all `[SMOD]` per-block logs
- **Attack logs**: Only skill attacks logged; normal attacks = zero output

### Errors
- `list_directory "gg_bypass2"` failed — correct path is `byp/gg_bypass2`
- User said "vc removeu errado" — the new `dec=/enc=` format was wrong; they wanted old format with stack traces. Fixed by reverting PacketSniffer.cpp to HEAD
- User identified the "polluting" pattern as the `LogRaw` hex dump (`[RECV] len=994 hex=... dec=...`), NOT the HookSend detailed logs. Fixed by removing only the `LogPacketDual`/`LogPacket` calls from `LogRaw`
- `NativeDecrypt` always fails for C2S (DecKey=0) — removed to eliminate per-packet noise

### Current State
- **Phase 1 (log consolidation + attack log reduction)**: ✅ COMPLETE
- **Phase 2 (packet sniffer format)**: ✅ COMPLETE
  - Old detailed format restored (stack trace, human-readable C3/C4, SMOD)
  - Polluting hex dump removed from `LogRaw`
  - `NativeDecrypt` removed; own `Decrypt` is the sole path
- **Build**: ✅ Last build succeeded, `byp.dll` deployed to `F:\blackrockmu\`
- **Diagnostics**: 0 errors on all modified files
- **Remaining**: None — all user requests addressed. The last build was successful and deployed.
---