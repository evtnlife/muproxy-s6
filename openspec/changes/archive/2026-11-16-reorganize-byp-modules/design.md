# Design

## Context

O byp.dll é uma DLL x86 injetada no cliente MU Online (ex603) que (1) faz bypass de
proteção, (2) faz sniffer/injeção de pacotes via hooks winsock, e (3) ataca monstros por
comando do MuProxy2 (C#) via TCP local. Ver proposal.md para a motivação.

Estado atual relevante para o desenho:

- **`PacketSniffer.cpp` (1577 linhas)** concentra 6 responsabilidades: hooks winsock
  (trampolines + `HookSend/Recv/WSA*/Connect`), reassembly TCP→pacotes MU, criptografia
  (Layer-1, `XorData`, `GetGameServerKeys`), dois canais TCP (44412 comandos / 44413
  pacotes), name-rewrite (HexDL) e injeção (`InjectToServer`/`InjectC1Attack`/
  `InjectSkillC1` + fila de skill).
- **Grafo de dependência** (quem inclui quem): `GameMemory` e `Logger` são folhas
  (nada depende deles no sentido inverso). O nó problemático é `PacketSniffer`:
  `AttackModule`, `dllmain` e (mortos) `DllNotifyHook`/`MainPatcher` apontam para ele.
- **Dois canais TCP distintos** (não é duplicação): `TcpClient` → 127.0.0.1:**44412**
  (comandos `CMD ATTACK...`); socket do `PacketSniffer` → 127.0.0.1:**44413** (log de
  pacotes `PKT` + comando `SEND`).
- **Código morto fora do build** (`build.bat` `SOURCES` lista 14 .cpp): a família PS.dll
  (`PsBypass`, `DllNotifyHook`, `MainPatcher`, `PsPacketHook`, `StartupHook`,
  `AsyncScanner`, `VPTracer`) + rascunho `PacketSniffer.cpp.new`.
- **Fila de skill é legado**: o skill agora vai direto por `InjectSkillC1`;
  `EnqueueSkillPacket`/`ProcessSkillQueue`/`SKILL_TYPE_*` são resíduo do caminho antigo
  (doc `byp/docs/skill_c1_07_19_fix/02_c3_pipeline.md`).
- **Contratos de injeção divergentes**: `InjectToServer` recebe a forma já XorData;
  `InjectC1Attack`/`InjectSkillC1` recebem plaintext. Três portas, três contratos.

Restrição dura: a ordem de instalação em `DllMain` é crítica — os hooks winsock são
instalados **sincronamente** em `DLL_PROCESS_ATTACH` para capturar o `connect` do jogo ao
game server; o resto roda em thread. Não se pode mudar essa ordem.

## Goals / Non-Goals

**Goals:**
- Um módulo por responsabilidade; nenhum arquivo > ~400 linhas.
- UMA porta de XOR (`MuCipher::XorData`) e UMA porta de injeção (`PacketInjector::Inject`).
- Remover todo o código morto (família PS.dll, rascunho, fila de skill).
- Preservar 100% do comportamento observável (bytes no wire, comandos, fluxo de injeção)
  e a ordem de instalação dos hooks em `DllMain`.
- Cada fase = 1 commit + build (`build.bat`) + teste em campo, para rollback granular.

**Non-Goals:**
- Não alterar offsets em `GameAddresses.h` (validados por RE).
- Não alterar a lógica de criptografia (só é movida, não reescrita).
- Não tocar em MuProxy2 (C#) nem no servidor MuEmu.
- Não introduzir novas dependências, frameworks ou padrões (ex.: DI, singletons globais).
- Não mudar a API pública usada por `dllmain.cpp` (`Install`/`Uninstall`/
  `Initialize`/`Shutdown`).

## Decisions

### D1 — Porta única de injeção por opcode (não herança)
**Decisão:** uma função `PacketInjector::Inject(const uint8_t* plaintext, int len)` que
aplica `MuCipher::XorData` uma vez e depois escolhe o pipeline pelo opcode no byte 2:
- `0x11` (normal) → Layer-1 → wire como C1 puro.
- `0x19` (skill) → SM input `[serial 0x08793700] + payload` → SimpleModulus (6→11) →
  `C3 0D [11]` → Layer-1 → wire.

**Racional:** o projeto é uma DLL de ~4k linhas; a regra real é "mesmo XOR, embalagem
final diferente". Um `switch` por opcode captura isso em ~30 linhas, sem cerimônia. O
XorData é aplicado em exatamente um lugar para todos os opcodes (o que o usuário pediu).

**Alternativa considerada (rejeitada):** hierarquia `AttackPacket` (base) com
`NormalAttack`/`SkillAttack` (subclasses). Mais extensível para muitos opcodes, mas
over-engineering para 2 opcodes; adiciona indirection e vtable num contexto de DLL
injetada onde simplicidade e depuração por log importam mais. Se aparecer um 3º opcode
com pipeline distinto, reavaliar — a migração para a hierarquia seria local.

### D2 — `MuCipher` como módulo de criptografia única
**Decisão:** novo par `MuCipher.h/.cpp` com `XorData`, `EncryptLayer1`/`DecryptLayer1`,
`FullEncrypt`/`FullDecrypt`, `GetGameServerKeys`, `GetMuHeaderSize`. `XorData` sai de
`static` (hoje file-local em `PacketSniffer.cpp`) e vira `MuCipher::XorData`, visível a
qualquer módulo. `SimpleModulusCipher` permanece classe separada (já está limpo) e é
chamado por `PacketInjector`, não por `MuCipher`.

**Racional:** concentra a cifra num lugar; qualquer pacote novo chama
`MuCipher::XorData(buf, 3, len)` e decide C1 ou SM→C3. Separa SM de Layer-1/XorData
porque SM só se aplica ao caminho C3 (skill), não é "camada" genérica.

**Alternativa (rejeitada):** manter tudo em `PacketSniffer`. Mantém o god file.

### D3 — Decomposição do god file em 5 módulos
**Decisão:**
| Módulo | Responsabilidade | Origem (linhas atuais) |
|---|---|---|
| `WinsockHooks` | trampolines, `InstallInlineHook`, `HookSend/Recv/WSA*/Connect/WSAConnect`, `Install`/`Uninstall` | ~687–1140 |
| `PacketReassembler` | buffers por socket, `ReadMuPacketLength`, `FlushReasmBuffer` | ~502–600 |
| `CommandChannel` | TCP 44412 (comandos) — absorve o papel de `TcpClient` | ~57–330 (parte) |
| `PacketChannel` | TCP 44413 (log de pacotes + `SEND`) | ~57–330 (parte) |
| `PacketInjector` | `Inject` (D1), pipelines, injeção no socket do GS | ~1400–1764 |
| `NameRewriter` | `EncodeName`, `IsCreateCharPacket`, g_RewriteName | ~655–684 |

`PacketSniffer.cpp` vira um fino orquestrador (ou some, com `dllmain` chamando os
módulos diretamente). `Install()` continua a entrada pública que `dllmain` chama.

**Racional:** um módulo por responsabilidade; cada um testável/legível isoladamente.
Separar `CommandChannel` de `PacketChannel` torna explícito que são dois canais distintos
(44412 vs 44413), removendo a ambiguidade que existia no `g_TcpSocket` único.

**Alternativa (rejeitada):** manter `TcpClient` separado e só extrair o resto. Deixaria
3 módulos fazendo TCP (TcpClient + 2 canais) com sobrepósito.

### D4 — Ferramentas de debug/RE saem do `AttackModule`
**Decisão:** mover `DumpObjectManager`, `HexDumpSlot`, `ScanForValue`, `ScanSlotForValue`,
`DirScan`, `SafeRead*` (~400 linhas) para `DebugTools.cpp` (namespace `DebugTools`),
registrado como comando opcional do `PacketHandler`. `AttackModule` fica com: config,
threads (Watcher/Dispatcher/SingleMob), build de pacotes, handlers de comando.

**Racional:** essas funções são ferramentas de RE interativas, não lógica de ataque.
Separá-las reduz o `AttackModule` à sua responsabilidade real.

**Alternativa (rejeitada):** apagar. O usuário ainda usa `CMD ATTACK DUMP`/`SCAN` em
campo; mover preserva a funcionalidade.

### D5 — `GameMemory` em sub-namespaces (não classes)
**Decisão:** reorganizar o namespace flat em sub-namespaces `GameMemory::Object`,
`GameMemory::Player`, `GameMemory::Globals`, mantendo os nomes das funções. Ex.:
`GameMemory::GetPlayerLevel()` → `GameMemory::Player::Level()` (ou manter nome e só
agrupar por seção de comentário, se o rename for arriscado).

**Racional:** 40 getters inline num namespace único é difícil de navegar. Sub-namespace
agrupa sem introduzir objetos/estado (as funções são stateless reads de memória).

**Alternativa (rejeitada):** classes `Object`/`Player` com métodos. As funções são
stateless e operam por index/endereço; classe não agregaria estado útil.

### D6 — Remover código morto (não arquivar)
**Decisão:** apagar a família PS.dll (7 .cpp + 7 .h), `PacketSniffer.cpp.new` e a fila de
skill (`EnqueueSkillPacket`, `ProcessSkillQueue`, `SKILL_TYPE_*`). Atualizar `build.bat`
(novos .cpp em `SOURCES`; mortos já não estão).

**Racional:** memória do projeto já registra "PS (PS.dll) no longer exists — related docs
can be deleted". Git preserva o histórico; manter os arquivos só polui a busca.

**Alternativa (rejeitada):** mover para `src/legacy/`. Mantém poluição sem benefício
(histórico já está no git).

## Risks / Trade-offs

- **[Extração dos hooks winsock quebra a captura do connect]** → Mitigação: a fase dos
  hooks é a ÚLTIMA e isolada; `Install()` mantém a mesma assinatura e a mesma ordem de
  chamadas em `DllMain` (síncrono no attach). Teste em campo obrigatório: conferir no log
  que o `connect` do GS é capturado após o refactor.
- **[Renomear/mover funções quebra includes espalhados]** → Mitigação: cada fase é um
  commit; o build (`cl.exe /W3`) pega include quebrado imediatamente. Fases pequenas
  limitam o blast radius.
- **[Dois canais TCP: confundir 44412/44413 na extração]** → Mitigação: nomes
  explícitos `CommandChannel`/`PacketChannel` + comentário de porta em cada um; teste em
  campo confirma que comandos e log de pacotes continuam nos canais certos.
- **[`skip_specs` esconde uma mudança de comportamento acidental]** → Mitigação: o
  critério de aceite é "bytes no wire idênticos". Cada fase com teste em campo (ataque
  normal + skill) valida isso antes de commit.
- **[Trade-off: porta única por opcode (D1) é menos extensível que herança]** → Aceito:
  para 2 opcodes o `switch` é mais claro; reavaliar se um 3º pipeline distinto aparecer.

## Migration Plan

Ordem por risco crescente (cada fase = 1 commit + `build.bat` + teste em campo):

1. **Fase 0 — Limpeza:** apagar família PS.dll + `PacketSniffer.cpp.new` + fila de skill;
   corrigir comentários obsoletos (`AttackModule.h` 0xEFF5). Build + smoke test.
2. **Fase 1 — `MuCipher`:** extrair criptografia; `XorData` vira público. Build + teste.
3. **Fase 2 — `DebugTools`:** extrair ferramentas de RE do `AttackModule`. Build + teste.
4. **Fase 3 — `NameRewriter` + `PacketReassembler`:** extrações de baixo risco. Build.
5. **Fase 4 — `PacketInjector` + porta única (D1):** unificar injeção. Build + teste
   normal/skill.
6. **Fase 5 — `CommandChannel`/`PacketChannel`:** separar os dois canais TCP. Build +
   teste (comandos + log).
7. **Fase 6 — `WinsockHooks` (a mais arriscada):** extrair hooks; preservar ordem em
   `DllMain`. Build + teste de captura do connect.
8. **Fase 7 — `GameMemory` sub-namespaces (D5):** reorganizar sem mudar comportamento.
   Build + teste.

**Rollback:** cada fase é um commit independente; `git revert <fase>` desfaz só aquela.
Como não há mudança de comportamento, o rollback não afeta dados nem wire.

## Open Questions

- **D1 (porta única vs herança):** o usuário foi questionado em explore e seguiu para
  propose sem objeção; assumida a porta única. Se preferir a hierarquia
  `NormalAttack`/`SkillAttack`, o escopo da Fase 4 muda (subclasses em vez de `switch`)
  — confirmar antes de aplicar a Fase 4.
- **D5 (renomear vs só agrupar):** renomear `GetPlayerLevel` → `Player::Level` é mais
  limpo mas toca mais call-sites. Decidir na Fase 7 se o rename vale o churn; o
  agrupamento por seção de comentário é o fallback de baixo risco.
