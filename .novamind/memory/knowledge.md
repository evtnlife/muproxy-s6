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

## Knowledge Consolidation - 2026-11-16T01:45:52.851Z

## Resumo da conversa

**Tarefa:** aplicar o change OpenSpec `reorganize-byp-modules` em `F:\blackrockmu` (cliente BlackrockMU), reorganizando o `byp.dll` (C++ x86) em módulos por responsabilidade, sem alterar comportamento observável. Resposta obrigatória em português.

### Change OpenSpec lido
- `F:\blackrockmu\openspec\changes\reorganize-byp-modules\proposal.md`, `design.md`, `tasks.md`, `.openspec.yaml`

### Estrutura final (decisão do usuário: sem `.cpp` solto na raiz; cada namespace em arquivo próprio)
Headers em `src\include\`; implementações em `core\`, `crypto\`, `net\`, `net\winsock\`, `packets\`, `game\`, `game\GameMemory\`, `attack\`, `debug\`.

### Arquivos criados
`F:\blackrockmu\byp\gg_bypass2\src\include\{CommandParams.h, DebugTools.h, NameRewriter.h, PacketReassembler.h, PacketInjector.h, OriginalApi.h, CommandChannel.h, PacketChannel.h, WinsockHooks.h, StackTrace.h, GameServerConnection.h, C3Decoder.h, GameMemory\Globals.h, GameMemory\Object.h, GameMemory\Player.h}`
`F:\blackrockmu\byp\gg_bypass2\src\debug\{DebugTools.cpp, DebugScans.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\packets\{NameRewriter.cpp, PacketReassembler.cpp, PacketInjector.cpp, GameServerConnection.cpp, PacketSniffer.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\net\{CommandChannel.cpp, PacketChannel.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\net\winsock\WinsockHooks.cpp`
`F:\blackrockmu\byp\gg_bypass2\src\core\{OriginalApi.cpp, StackTrace.cpp, dllmain.cpp, Logger.cpp, InlineHook.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\crypto\{MuCipher.cpp, SimpleModulusCipher.cpp, C3Decoder.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\game\GameMemory\{Globals.cpp, Object.cpp, Player.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\game\{MonsterTracker.cpp, ViewportMap.cpp, SkillMap.cpp, SkillHook.cpp}`
`F:\blackrockmu\byp\gg_bypass2\src\attack\AttackModule.cpp`

### Arquivos movidos (git rename)
`src\{dllmain,Logger,InlineHook}.cpp`→`core\`; `src\SimpleModulusCipher.cpp`→`crypto\`; `src\{TcpClient,PacketHandler,MessageRegistry}.cpp`→`net\`; `src\{PacketSniffer,PacketReassembler,NameRewriter}.cpp`→`packets\`; `src\{GameMemory,MonsterTracker,ViewportMap,SkillMap,SkillHook}.cpp`→`game\`; `src\AttackModule.cpp`→`attack\`; `src\include\TcpClient.h`→`include\CommandChannel.h`; `src\game\GameMemory.cpp`→`game\GameMemory\Player.cpp`.

### Arquivos modificados
`F:\blackrockmu\byp\gg_bypass2\build.bat` (SOURCES por pasta), `src\include\PacketSniffer.h`, `src\include\GameMemory.h` (removido), `src\game\GameMemory.cpp` (removido), `src\attack\AttackModule.cpp`, `src\game\MonsterTracker.cpp`, `src\game\SkillHook.cpp`, `src\core\dllmain.cpp`, `src\net\PacketHandler.cpp`, `src\crypto\{MuCipher,SimpleModulusCipher}.cpp`, `src\debug\{DebugTools,DebugScans}.cpp`, `src\packets\PacketSniffer.cpp`, `F:\blackrockmu\openspec\changes\reorganize-byp-modules\tasks.md`, `F:\blackrockmu\byp.dll` (artefato).

### Arquivos lidos (não modificados)
`F:\blackrockmu\byp\gg_bypass2\check_imports.bat`; `src\include\{Config.h, GameObject.h, MessageRegistry.h, PacketHandler.h, PacketSniffer.h, AttackModule.h, sdk.h, SimpleModulusCipher.h, MuCipher.h, DebugTools.h, TcpClient.h}`; `src\{MessageRegistry, PacketHandler, Logger, TcpClient}.cpp`; `src\game\SkillHook.cpp`; `F:\blackrockmu\byp\MuProxy2\MuProxy2\Protocol\{Messages.cs, MonsterAttacker.cs}`; `F:\blackrockmu\byp\MuProxy2\MuProxy2\Services\{ServiceHub.cs, DllCommandService.cs}`; `F:\blackrockmu\byp\MuProxy2\MuProxy2\MainForm.cs`.

### Decisões-chave
- `MuCipher` = módulo único de cifra; XorData aplicado uma vez; porta única `PacketInjector::Inject` (0x11→Layer-1; 0x19→SM+serial→C3 0D→Layer-1) + `InjectXorData` para dados já cifrados; transporte via callback.
- `DebugTools` extraído do AttackModule (tag de log mantida "ATTACK"); comandos `ATTACK_DUMP/HEXDUMP/SCANVALUE/SCANSLOT/DIRSCAN` via MessageRegistry; parsing extraído para `CommandParams.h`.
- `NameRewriter` preservado **dormante** (sem call-site, não ativado).
- `PacketReassembler` com callback; branch morto de ViewportMap/SkillMap mantido inerte.
- `TcpClient`→`CommandChannel` (44412); `PacketChannel` novo (44413); `OriginalApi` guarda trampolines (send/connect) para os canais não passarem pelo hook.
- `WinsockHooks` extraído com observers; `PacketSniffer` virou orquestrador; hooks continuam síncronos em `DLL_PROCESS_ATTACH` (ordem do DllMain preservada).
- Strings de log preservadas (ex.: `InjectSkillC1: after XorData`) para comparação em campo.

### Erros e correções
- `run_command` com cwd relativo → ENOENT; resolvido executando da raiz.
- `git show` não existe na tool git → usado `run_command`.
- Primeiro commit varreu `.rar` de 191 MB e deleções `.novamind` → `git commit --amend` removeu/restaurou.
- `MAKE_VERSION` inexistente → `MAKEWORD(2,2)`.
- Detecção C116 reimplementada por engano → recuperada do `git show` e copiada verbatim (porta big-endian).
- `GameMemory.h` removido com include pendente em `PacketHandler.cpp` → C1083 → include morto removido.
- `Player::Object()` sombreava o namespace `Object` → qualificado `GameMemory::Object::Array()`.
- Bloco TCP não removido por edição fuzzy → releitura e remoções em blocos.
- Caracteres chineses acidentais em comentário → corrigidos.

### Estado atual
Commits: `c92afdc` (Fase 1 MuCipher), `de2c6f8` (Fase 2 DebugTools), `49f42fb` (Fase 3), `eb7708d` (pastas), `406eb85` (Fase 4), `f4b7b44` (Fase 5), `6c16147` (Fase 6), `3730e71` (Fase 7), `5becc0a` (integração final), `6846467` (tasks.md). Build via `cmd /c "byp\gg_bypass2\build.bat"` → SUCESSO, apenas warnings pré-existentes (C4996 `inet_ntoa`, C4530 `/EHsc`); `byp.dll` implantado em `F:\blackrockmu`. Árvore limpa. Restam apenas **testes de campo** do usuário: comparar bytes 0x11/0x19 com os anteriores, conectividade com o MuProxy2 e os comandos de debug; a próxima ação pendente era salvar memória do projeto sobre a nova estrutura.
---