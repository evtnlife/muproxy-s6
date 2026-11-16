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

## Knowledge Consolidation - 2026-11-16T02:28:13.895Z

## Objetivo e estado geral

Projeto `F:\blackrockmu` (BlackrockMU): byp.dll (C++ x86) + MuProxy2 (C#). Duas frentes concluídas: (1) refactor `reorganize-byp-modules`; (2) implementação de **auto-potion** (`add-auto-potion`, commit `964db9d`). Respostas obrigatoriamente em português.

## Refactor anterior (commits `c92afdc`…`6846467`)
Arquitetura final: headers em `F:\blackrockmu\byp\gg_bypass2\src\include\`, implementações em `src\core\`, `src\crypto\`, `src\net\`, `src\net\winsock\`, `src\packets\`, `src\game\`, `src\game\GameMemory\`, `src\attack\`, `src\debug\` (sem `.cpp` na raiz). Criados: `src\include\{CommandParams.h,DebugTools.h,NameRewriter.h,PacketReassembler.h,PacketInjector.h,OriginalApi.h,CommandChannel.h,PacketChannel.h,WinsockHooks.h,StackTrace.h,GameServerConnection.h,C3Decoder.h,GameMemory\Globals.h,GameMemory\Object.h,GameMemory\Player.h}`, `src\core\{OriginalApi.cpp,StackTrace.cpp,dllmain.cpp,Logger.cpp,InlineHook.cpp}`, `src\crypto\{MuCipher.cpp,SimpleModulusCipher.cpp,C3Decoder.cpp}`, `src\net\{CommandChannel.cpp,PacketChannel.cpp}`, `src\net\winsock\WinsockHooks.cpp`, `src\packets\{NameRewriter.cpp,PacketReassembler.cpp,PacketInjector.cpp,GameServerConnection.cpp,PacketSniffer.cpp}`, `src\game\GameMemory\{Globals.cpp,Object.cpp,Player.cpp}`, `src\game\{MonsterTracker.cpp,ViewportMap.cpp,SkillMap.cpp,SkillHook.cpp}`, `src\attack\AttackModule.cpp`, `src\debug\{DebugTools.cpp,DebugScans.cpp}`. Movidos: `src\{dllmain,Logger,InlineHook,SimpleModulusCipher,TcpClient,PacketHandler,MessageRegistry,PacketSniffer,PacketReassembler,NameRewriter,GameMemory,MonsterTracker,ViewportMap,SkillMap,SkillHook,AttackModule}.cpp`; `src\include\TcpClient.h`→`CommandChannel.h`. Removidos: `src\include\GameMemory.h`, `src\game\GameMemory.cpp`. Modificados: `build.bat`, `src\include\PacketSniffer.h`, `src\attack\AttackModule.cpp`, `src\game\{MonsterTracker.cpp,SkillHook.cpp}`, `src\core\dllmain.cpp`, `src\net\PacketHandler.cpp`, `src\crypto\{MuCipher.cpp,SimpleModulusCipher.cpp}`, `src\debug\{DebugTools.cpp,DebugScans.cpp}`, `src\packets\PacketSniffer.cpp`, `F:\blackrockmu\openspec\changes\reorganize-byp-modules\tasks.md`, `F:\blackrockmu\byp.dll`. Build OK (só warnings pré-existentes C4996/C4530).

## Skill `.novamind` (commit `d18dcebc`)
Corrigidas obsoletações: PacketSniffer agora orquestrador; família PS.dll inexistente; `TcpClient`→`CommandChannel`/`PacketChannel`; `IsControlSocket`→`OriginalApi`; `GameServer*`→`MuCipher::*`; `InjectC1Attack`/`InjectToServer`/`EnqueueSkillPacket`→`PacketInjector::Inject`/`InjectXorData`; **0x19 corrigido** (skill primeiro, index puro, sem direção, sem `0xEFF5`); `INJECT_PACKET` removido; paths atualizados. Criado `F:\blackrockmu\.novamind\skills\blackrockmu\references\12-module-map.md`; modificados `SKILL.md`, `references\{01-architecture.md,02-channels.md,03-crypto.md,04-attack-packet.md,05-object-array.md,06-message-protocol.md,07-build-deploy.md,09-golden-rules-debug.md,10-references.md,11-log-analysis.md}`, `evals\{add-new-message-tag,debug-corrupted-attack-packets,rebuild-after-offset-change}\prompt.md`.

## Auto-potion (commit `964db9d`)
Pesquisa (lidos): `F:\blackrockmu\MuEmu-master\eMU\GameServer\GameServer\{Protocol.cpp,ItemManager.h,ItemManager.cpp,ObjectManager.cpp,PacketManager.cpp,ServerInfo.h,ServerInfo.cpp,User.h}`, `F:\blackrockmu\MuEmu-master\Main_EX603\Main\{Item.cpp,Object.h,Protocol.h,Protocol.cpp,PacketManager.h,PacketManager.cpp,User.h}`, `F:\blackrockmu\OpenMU-master\docs-website\docs\server-features\cash-shop.md`, `F:\blackrockmu\byp\docs\skill_c1_07_19_fix\{01_xor_decode_and_fix.md,02_c3_pipeline.md}`, `F:\blackrockmu\byp\docs\stack_c1_07_19\{00_overview.md,03_send_encrypt_0x404DB0.md}`. Conclusões: uso de item = `C1 06 26 <SourceSlot> <TargetSlot> <type>`; `CGItemUseRecv`→`CharacterUsePotion` (potions `GET_ITEM(14,0..6)`, `14,35..40`, `14,70/71/133`); `SourceSlot==TargetSlot` rejeitado; anti-hack por `CheckAutoPotionHackTolerance`; **XorData [3,size) aplica-se a todos C1** (servidor sempre decifra). Divergência aberta: SM 6 bytes (nosso) vs 4 bytes (stack RE).

Implementado: `F:\blackrockmu\byp\gg_bypass2\src\include\{MuOpcodes.h,MuCipher.h,PacketInjector.h,AutoPotion.h,Config.h}`, `src\crypto\MuCipher.cpp` (`UnXorData` inverso), `src\packets\{PacketInjector.cpp,PacketSniffer.cpp}`, `src\potion\AutoPotion.cpp`, `src\core\dllmain.cpp`, `src\attack\AttackModule.cpp`, `build.bat`; MuProxy2: `Protocol\Messages.cs`, `Services\PotionService.cs`, `MainForm.cs`, `MainForm.Designer.cs`; OpenSpec: `openspec\changes\add-auto-potion\{.openspec.yaml,proposal.md,design.md,tasks.md}`, `openspec\specs\auto-potion\spec.md`; skill: nova `references\13-auto-potion.md` + updates em `SKILL.md`, `06`, `11`, `12`.

Decisões: `MuOpcodes` como enums tipadas por direção (`HeaderType`, `ClientToServer`, `ServerToClient`, `AttackType`, `Size`); `PacketInjector` com um pipeline por opcode e XorData aplicado uma única vez; auto-potion com watcher 100 ms, thresholds hp=50/mp=30, intervalo 1000 ms, comandos `POTION_START/STOP/CONFIG/STATUS`, modelo "learn & replay" (observa `C1 06 26` do jogador, classifica HP/MP pela variação de life/mana) e slot manual como fallback.

## Erros e correções
`edit_file` falhou por caracteres chineses acidentais em `design.md` (relido e corrigido); `write_file` recusado em `PacketInjector.h` (usado `edit_file`); `search_files` ignora `path`/`include` e satura em 40 hits (refinou buscas); colisão `Config` vs `AutoPotion::Config` → struct renomeado `Settings`; `constexpr`→`enum class` exigiu casts em `AttackModule.cpp`; aviso de deploy "cannot find the file specified" = cópia de `opengl32.dll` ausente (pré-existente; `byp.dll` copiado).

## Estado atual / pendências
Build C++ limpo e `dotnet build` OK (0 warnings). Commits: `d18dcebc`, `964db9d`; restam apenas arquivos auto-gerados de memória `.novamind` modificados. Pendente do usuário: **testes de campo** — beber HP/MP manualmente uma vez para registrar os slots, validar bytes injetados (0x26), limiares/intervalo, e então arquivar o change.
---

## Knowledge Consolidation - 2026-11-16T11:34:12.099Z

## Resumo da sessão (BlackrockMU — `F:\blackrockmu`)

### Objetivo
Diagnosticar por que **toda skill injetada causava DC** no jogo, revisar o git, corrigir a regressão e depois fazer o **auto-potion** funcionar (configuração por **ID de item** em vez de slot). Respostas em português. **Regra do usuário: não fazer commit.**

### Diagnóstico da DC (skill)
- **Causa raiz provada:** `SimpleModulusCipher::EncryptBlock` cifra **8 bytes** do source (word 3 da chave SM vem de 2 bytes *além* do buffer de 6 bytes = lixo de stack), mas o checksum assumia **zeros** → pacote **autoinconsistente** → servidor falha o checksum do SM → derruba a conexão.
- Prova (decifra validada contra o log do jogo): nativo `C3 0D 25 8F…` → `calc=0xBC expected=0xBC` OK; **injetado `8D 1F…` → `calc=0x41 expected=0xDF` FALHOU**.
- Bug introduzido em `5330bda` (14/11) — intermitente (dependia do lixo de stack). Refactor (`c92afdc…5becc0a`) e auto-potion (`964db9d`) **não mudaram os bytes da skill**.
- Histórico: `c6ed86c` trocou a fila que usava o **builder nativo** (`0x404B40/0x404B90/0x4050D0/0x404D70`) pelo C3+SM manual; `e0945a2` removeu a fila como "código morto"; docs `byp/docs/skill_c1_07_19_fix/*` ficaram "Pendente (validação em campo)".
- Regressão de log do `964db9d`: `InjectSkillC1: sent … skill=0x…` passou a mostrar valores Camada-2 (`0xE7C3`) em vez do plaintext (`0x003C`) — corrigida.
- Capturas do usuário revelaram a regra: **uso de item (0x26) também vai dentro de C3+SM** (`C1 06 26 E9 F1 D2`, SM source `[serial]+innerC1[2..size]`, numBytes = size−1). Skill = numBytes 6; item = 5.

### Crash ao ativar auto-potion
- Log do próprio byp: `ACCESS_VIOLATION` em `byp.dll`, pilha com `50/30/1000` (locals de `LogStatus`).
- `dumpbin` mostrou `mov edx,[eax+08128AC8h]` (ponteiro = tabela de skill wire IDs, não ObjectCharacter) e `mov cl,[eax+edx]` (leitura de `name[i]`).
- **Causa do SEH não segurar:** `/O2` move loads para fora do `__try` (`mov dword ptr [ebp-4],0` antes do load) e o projeto compilava **sem `/EHsc`** (aviso C4530).
- Solução: `SafeRead.h` com checagem explícita via `VirtualQuery` (sem dereferenciar), `/EHsc` no link, `/MAP` para mapear offsets, crash log com `ModuleBase`/`EIP_RVA`.

### Arquivos modificados/criados (todos em `F:\blackrockmu\`)
- `byp\gg_bypass2\src\crypto\SimpleModulusCipher.cpp` — checksum cobre os mesmos 8 bytes da cifra.
- `byp\gg_bypass2\src\packets\PacketInjector.cpp` — helper genérico `SendC3Sm` (skill + item via C3+SM), buffer SM de 8 bytes zerado, log em claro.
- `byp\gg_bypass2\src\packets\PacketSniffer.cpp` — decodifica C3 p/ ver `C1 06 26` interno (limite 22 bytes: corrigido overflow de 64→96 bytes), alimenta life/mana, log dos 30 primeiros pacotes S→C, chama `OnItemListPacket`.
- `byp\gg_bypass2\src\potion\AutoPotion.cpp` — TargetSlot `0xFF`→`0x00`; tabela slot→item ID (F3 10); `hpitem`/`mpitem`; `FindSlotByItemId`; `ReadVitals` (objChar/party list/tráfego) com Camada-2 inversa; `LogSourcesOnce`.
- `byp\gg_bypass2\src\include\AutoPotion.h` — novas APIs e `hpItemId/mpItemId`.
- `byp\gg_bypass2\src\include\SafeRead.h` — **novo** (VirtualQuery + helpers).
- `byp\gg_bypass2\src\game\GameMemory\Player.cpp` / `include\GameMemory\Player.h` — leituras seguras (SEH+SafeRead), stats movidas do header, `PartyLife/MaxLife/LifePercent`.
- `byp\gg_bypass2\src\include\GameAddresses.h` — `PARTY_LIST_PTR = 0x81CB4E8`.
- `byp\gg_bypass2\src\core\dllmain.cpp` — crash log com base do módulo e RVA.
- `byp\gg_bypass2\build.bat` — `/EHsc` e `/MAP:..\dist\byp.map`.
- `byp\MuProxy2\MuProxy2\Protocol\Messages.cs`, `byp\MuProxy2\MuProxy2\Services\PotionService.cs` — `hpitem`/`mpitem`.
- `byp.dll` (raiz) e `byp\gg_bypass2\dist\byp.dll` — rebuild/deploy.

### Arquivos lidos (principais)
`MuOpcodes.h`, `PacketInjector.h/.cpp`, `AttackModule.cpp`, `SkillHook.cpp`, `SimpleModulusCipher.cpp/.h`, `C3Decoder.cpp`, `MuCipher.cpp/.h`, `OriginalApi.cpp`, `WinsockHooks.cpp`, `GameServerConnection.cpp`, `PacketSniffer.cpp`, `AutoPotion.cpp/.h`, `Player.cpp/.h`, `CommandParams.h`, `CharacterStruct.h`, `UserObjectStruct.h`, `GameObject.h`, `PartyList.h`, `GameAddresses.h`, `SimpleModulusCipher.h`; C#: `PotionService.cs`, `MainForm.cs`, `Messages.cs`, `PacketType.cs`, `TcpLoopbackProxy.cs`; docs: `byp\docs\skill_c1_07_19_fix\01_xor_decode_and_fix.md`, `…\02_c3_pipeline.md`, `byp\docs\stack_c1_07_19\{00_overview,01_builder_0x59C260,03_send_encrypt_0x404DB0}.md`, `byp\docs\skill_0x19_diff_analysis.md`; servidor ref.: `MuEmu-master\eMU\GameServer\GameServer\{Protocol.cpp,Protocol.h,ItemManager.cpp,ItemManager.h,ObjectManager.cpp,SkillManager.cpp,PacketManager.cpp}`, `MuEmu-master\Main_EX603\Main\{Protocol.cpp,Protocol.h,Object.h,PacketManager.cpp}`; logs: `F:\blackrockmu\gg_bypass2.log`, `byp\MuProxy2\MuProxy2\bin\Debug\net8.0-windows\logs\muproxy2_20261115_182430.log`, `byp\MuProxy2\MuProxy2.Tests\bin\Release\net8.0-windows\byp_packets.log`, `byp\gg_bypass2\dist\byp.map`.

### Erros e correções
Redirecionamento PowerShell gravou UTF-16 (diff binário) → `git diff` direto; `edit_file` com CRLF não casou → Python/repl; `dumpbin` fora do PATH → caminho completo do VS; `dotnet build` sem csproj → `MuProxy2.csproj`; `OFF_DAMAGERMIN` inexistente → `OFF_DAMAGEMIN/MAX`; buffer overflow 64 vs 96 bytes; SEH inefetivo por `/EHsc` + hoisting de loads.

### Estado / pendências
Build C++ limpo (C4530 eliminado), `dotnet build` 0 erros/0 warnings, DLL implantada, **sem commit**. Crash corrigido (fonte: load fora do `__try` → `SafeRead`). Falta: (1) teste de campo — `POTION_START` não deve crashar e a potion deve ser usada; (2) obter do usuário as linhas `S->C #N:` e `POTION: fontes:` para fechar o parse real do inventário (`F3 10`) e a origem do life/mana; (3) expor `hpitem/mpitem` na UI do MuProxy2; (4) skill ainda não validada em campo.
---

## Knowledge Consolidation - 2026-11-16T12:37:33.299Z

]<]minimax[>[<tool_call>
[Tool: run_command]
Command: cd F:\blackrockmu\byp\gg_bypass2; cmd /c build.bat 2>&1 | Select-String -Pattern 'error|SUCESSO|ERRO|DEPLOY' | Select-Object -First 10
Cwd: f:\blackrockmu

SUCESSO: byp.dll criada em ..\dist\byp.dll
Deploy para F:\blackrockmu\...
DEPLOY OK
---

## Knowledge Consolidation - 2026-11-16T14:37:04.050Z

Justo — a change sem UI é change pela metade. Implementei a **Fase 6**.

## O que entrou

**byp (`AutoPotion.cpp`)** — o módulo agora manda o estado para o painel, pelo canal de comandos, ~1x/s:

```
POTION_STATE|active=1|hp=12|hpSrc=inv|mp=39|mpSrc=inv|items=47|life=202|lifeMax=214|mana=14|manaMax=219|warn=
```

O `hpSrc`/`mpSrc` diz **de onde veio o slot**: `inv` (inventário), `cfg` (digitado na UI), `apr` (modelo aprendido) ou `-`.

**C# (`MuProxy2`)**:
- `PotionStateMessage` (novo, tag `POTION_STATE`) em `Protocol/Messages.cs`
- `DllCommandService.OnPotionState` (evento novo) + dispatch no `ProcessLine`
- `MainForm` atualiza um label novo no rodapé do grupo de potion (`lblPotionState`):

```
HP 202/214 (94%)  slot 12 (inv)    MP 14/219 (6%)  slot 39 (inv)
LIGADO | inventario: 47 itens
```

E em **vermelho** quando falta potion de algum tipo (`warn=`) — tipo `sem HP` / `sem MP`. Ou seja: o painel passa a mostrar **se o módulo está vendo o dano** e **qual slot ele está usando**, sem precisar ir caçar no log.

## Estado dos builds

- `byp.dll`: **OK** (deploy feito)
- `MuProxy2.exe`: **OK** (0 warnings / 0 errors), reiniciado (PID 10924)
- `openspec/changes/auto-potion-autonomo/tasks.md`: Fase 6 marcada

## O que falta é só o teste de campo

O build mata o `main.exe`, então **sobe o jogo** e olha o log:

```
[POTION] C4-SM decodificado ...              ← o C4 de 784 bytes (o inventário)
[POTION] lista de itens (F3 10): 47 item(ns)
[POTION] pocoes no inventario: 3 slot(s) de HP, 2 de MP
```

Se a linha `lista de itens` aparecer, o slot vem do inventário (não precisa beber nada) e a UI deve mostrar `slot 12 (inv)` sozinho. Se aparecer `-` na UI, é porque o inventário ainda não chegou — me manda a linha do log do `C4` que eu ajusto.

Observação: o `F3 03` (character info) que vem no C3 de 233 bytes tem layout **clássico** (WORDs big-endian), diferente do `F3 E0` que o byp parseia; como o `F3 E1` já entrega `maxLife/maxMana` (214/219), deixei o `F3 03` de fora por enquanto — se o `max` em algum momento não chegar, esse é o ponto a ajustar.
---