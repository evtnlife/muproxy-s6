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

## Knowledge Consolidation - 2026-11-15T18:26:56.243Z

## Summary

**Original Task:** Walk the C1 07 19 skill-attack send stack trace in x32dbg, disassemble each function with full ASM + comments, and document step-by-step to reconstruct the packet builder structure.

**Stack Trace (return addresses):**
`Main.dll+0xBB3DE → main.exe+0x5153 → +0x4EF9 → +0x4D9A → +0x19C7A7 → Main.dll+0x1484F1 → main.exe+0x19BA17 → +0x1B554F → +0x1B7745 → +0xD957C → Main.dll+0x1AD16 → main.exe+0xDADA9 → +0xD34A4 → +0x5DB688 → KERNEL32 → ntdll`

**Module bases:** main.exe = 0x400000, Main.dll = 0x67C20000

**Files created (all in `F:\blackrockmu\byp\docs\stack_c1_07_19\`):**
- `00_overview.md` — stack map, key findings, module bases
- `01_builder_0x59C260.md` — full builder function (prologue, SEH, skill dispatch switch, packet assembly at 0x59C713–0x59C7A2)
- `02_packet_obj_methods.md` — Reset(0x404B40), SetHeader(0x404B90), InsertAt(0x404C10), checksum(0x404C90), AddByte(0x4050D0); packet layout: `[this+0]=WORD len, [this+2]=data[0x800]`
- `03_send_encrypt_0x404DB0.md` — SendEncrypted: reads serial from 0x08793700, overwrites data[3], calls SM EncryptBlock, prepends C3 header
- `04_thunk_0x59B3C0.md` — 5-byte jmp to Main.dll+0x148330 (0x67D68330)

**Key architectural findings:**
- Builder at 0x59C260: large function (~0x1000+ bytes), SEH prologue, dispatches on skill ID, writes C1 07 19 header + skill_high/low + target_high/low into local buffer at [ebp-0x82C]
- Send fn 0x404DB0: overwrites data[3] with serial (MAIN_PACKET_SERIAL at 0x08793700), then SM-encrypts
- The "skill wire 0xE7C3" was a misread — it's actually serial(E7) + skill_low(C3)
- 0x404C90 builds a 32-byte lookup table (checksum/CRC)

**Live breakpoint capture (Phase 2, in progress):**
- BP1 at 0x59C725 hit: skill=0x3C (Force), target_obj=0x1016A434, target_ID=0x00D7 (word at [obj+0x7E])
- BP2 at 0x404E34 hit: serial=0x09 (al), global [0x08793700]=0x09, but buffer showed `C1 07 19 E7 C3 E2 51` — **different packet** (game auto-attacks rapidly; BPs catch different packets)
- Continue was issued; debugger is currently "stopped" (likely hit BP1 again on next auto-attack)

**Pending phases (from plan):**
- Phase 2: Resolve the buffer mismatch (confirm BPs hit different packets), capture a matched pair
- Phase 3: Disassemble upper frames: main.exe+0x1B554F, +0x1B7745, +0xD957C, +0xDADA9, +0xD34A4, +0x5DB688
- Phase 4: Disassemble Main.dll function at 0x67D68330 (thunk target)
- Phase 5: Update 00_overview.md with live values, remove breakpoints

**Errors encountered:**
- `xdbg_pause` returned "ok" but state stayed "running" — worked around by reading memory while running (x32dbg allows memory reads during execution)
- Pattern searches for function prologue (`55 8B EC 81 EC`, `55 8B EC C1 EC`) failed in narrow ranges — resolved by searching for `CC CC CC CC 55 8B EC 6A FF` (int3 padding + prologue) which found the start at 0x59C260
- `break` command invalid in x32dbg command bar

**Rules added to memory:**
- Workflow: write a small separate doc per step IMMEDIATELY after each analysis step, one step at a time
- Technical: C1 07 19 "skill wire 0xE7C3" was misread; Send fn overwrites data[3] with serial
---