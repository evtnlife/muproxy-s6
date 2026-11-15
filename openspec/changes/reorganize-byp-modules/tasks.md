# Tasks

> Cada grupo = 1 fase do Migration Plan (design.md) = 1 commit + `build.bat` + teste em
> campo. Verificação está embutida em cada task. Ordem por risco crescente.

## 1. Fase 0 — Limpeza de código morto e comentários

- [ ] 1.1 Apagar a família PS.dll (`PsBypass`, `DllNotifyHook`, `MainPatcher`,
  `PsPacketHook`, `StartupHook`, `AsyncScanner`, `VPTracer` — .cpp + .h) e verificar que
  `build.bat` ainda compila (nenhum estava em `SOURCES`) e que não há `#include` restante.
- [ ] 1.2 Apagar o rascunho `src/PacketSniffer.cpp.new` e verificar que nenhuma referência
  a `.cpp.new` existe no build ou nos scripts.
- [ ] 1.3 Remover a fila de skill legado (`EnqueueSkillPacket`, `ProcessSkillQueue`,
  struct `SkillPacketEntry`, enum `SKILL_TYPE_*` de `PacketSniffer.h`) e verificar que o
  build passa e o skill continua indo por `InjectSkillC1` (teste em campo: cast de Force).
- [ ] 1.4 Corrigir comentários obsoletos (ex.: `AttackModule.h` linha ~34 "0xEFF5 ...
  ataques normais E skill") e verificar por busca que não resta menção a 0xEFF5 aplicado
  a skill 0x19.
- [ ] 1.5 Commit da Fase 0 e smoke test em campo (jogo abre, log sem erro, ataque normal
  funciona).

## 2. Fase 1 — Extrair `MuCipher` (criptografia única)

- [ ] 2.1 Criar `src/MuCipher.h` + `src/MuCipher.cpp` com `XorData`, `EncryptLayer1`/
  `DecryptLayer1`, `FullEncrypt`/`FullDecrypt`, `GetGameServerKeys`, `GetMuHeaderSize` e
  a tabela `g_XorFilter`; verificar que o arquivo compila isolado.
- [ ] 2.2 Adicionar `src/MuCipher.cpp` à `SOURCES` do `build.bat` e verificar que o build
  gera `byp.dll`.
- [ ] 2.3 Migrar os callers em `PacketSniffer.cpp` (e qualquer outro) para
  `MuCipher::*`, removendo as cópias locais; verificar que `XorData` só existe em
  `MuCipher` (busca por `g_XorFilter` retorna 1 resultado) e o build passa.
- [ ] 2.4 Commit da Fase 1 e teste em campo: ataque normal (0x11) e skill (0x19) produzem
  os mesmos bytes no wire que antes (conferir no log `InjectSkillC1: after XorData`).

## 3. Fase 2 — Extrair `DebugTools` do `AttackModule`

- [ ] 3.1 Criar `src/DebugTools.h` + `src/DebugTools.cpp` (namespace `DebugTools`) e mover
  `DumpObjectManager`, `HexDumpSlot`, `ScanForValue`, `ScanSlotForValue`, `DirScan`,
  `SafeReadDword/Word/Byte/Float`; verificar que o build passa.
- [ ] 3.2 Re-registrar os comandos de debug (`CMD ATTACK DUMP`, `CMD ATTACK SCAN`, etc.)
  apontando para `DebugTools` via `PacketHandler`/`MessageRegistry`; verificar que os
  comandos respondem no MuProxy2.
- [ ] 3.3 Adicionar `src/DebugTools.cpp` à `SOURCES` do `build.bat` e verificar o build.
- [ ] 3.4 Commit da Fase 2 e verificar que `AttackModule.cpp` ficou só com config,
  threads, build de pacotes e handlers (sem funções de dump/scan).

## 4. Fase 3 — Extrair `NameRewriter` e `PacketReassembler`

- [ ] 4.1 Criar `src/NameRewriter.h/.cpp` com `EncodeName`, `IsCreateCharPacket`,
  `g_NameKey`, `g_RewriteName`, `g_RewriteClass`; verificar que o build passa e o
  name-rewrite (HexDL) continua aplicado no pacote de create-char.
- [ ] 4.2 Criar `src/PacketReassembler.h/.cpp` com os buffers por socket,
  `ReadMuHeaderSize`/`ReadMuPacketLength`, `FlushReasmBuffer`; verificar que o build passa
  e o log de pacotes continua montando pacotes completos.
- [ ] 4.3 Adicionar os dois .cpp à `SOURCES` do `build.bat` e verificar o build.
- [ ] 4.4 Commit da Fase 3 e teste em campo: log de pacotes e name-rewrite íntegros.

## 5. Fase 4 — `PacketInjector` + porta única de injeção (D1)

> Confirmação pendente: D1 usa porta única `Inject(plaintext,len)` com `switch` por
> opcode. Se o usuário preferir a hierarquia `NormalAttack`/`SkillAttack`, este grupo vira
> subclasses (mesmo escopo, outra forma).

- [ ] 5.1 Criar `src/PacketInjector.h/.cpp` com `Inject(const uint8_t* plaintext, int len)`
  que aplica `MuCipher::XorData` uma vez e escolhe o pipeline pelo opcode (0x11 → Layer-1
  C1 puro; 0x19 → SM+serial → C3 0D → Layer-1); verificar que o build passa.
- [ ] 5.2 Migrar `AttackModule::SendAttack`/`SendSkillAttack` para chamar
  `PacketInjector::Inject`; verificar que o build passa.
- [ ] 5.3 Remover (ou reduzir a wrapper fino) `InjectC1Attack`/`InjectSkillC1`/
  `InjectToServer` antigos em `PacketSniffer.cpp`; verificar que só existe uma porta de
  injeção e o build passa.
- [ ] 5.4 Adicionar `src/PacketInjector.cpp` à `SOURCES` do `build.bat` e verificar o build.
- [ ] 5.5 Commit da Fase 4 e teste em campo: ataque normal e skill produzem os mesmos
  bytes no wire (conferir log).

## 6. Fase 5 — Separar `CommandChannel` (44412) e `PacketChannel` (44413)

- [ ] 6.1 Criar `src/CommandChannel.h/.cpp` (TCP 44412, comandos `CMD`) e
  `src/PacketChannel.h/.cpp` (TCP 44413, log `PKT` + `SEND`), separando o `g_TcpSocket`
  único atual; verificar que o build passa.
- [ ] 6.2 Atualizar `dllmain.cpp`/`TcpClient` para usar os dois canais; verificar que o
  build passa e não há socket duplicado.
- [ ] 6.3 Adicionar os dois .cpp à `SOURCES` do `build.bat` e verificar o build.
- [ ] 6.4 Commit da Fase 5 e teste em campo: comandos chegam por 44412 e o log de pacotes
  sai por 44413 (conferir portas no log).

## 7. Fase 6 — Extrair `WinsockHooks` (fase de maior risco)

- [ ] 7.1 Criar `src/WinsockHooks.h/.cpp` com trampolines, `AllocateTrampoline`,
  `InstallInlineHook`, `HookSend/Recv/WSASend/WSARecv/SendTo/RecvFrom/Connect/WSAConnect`
  e a completion routine overlapped; verificar que o build passa.
- [ ] 7.2 Preservar a ordem síncrona de `Install()` em `DllMain` (`DLL_PROCESS_ATTACH`) —
  os hooks devem instalar antes de qualquer thread; verificar por inspeção que a ordem em
  `dllmain.cpp` não mudou.
- [ ] 7.3 Adicionar `src/WinsockHooks.cpp` à `SOURCES` do `build.bat` e verificar o build.
- [ ] 7.4 Commit da Fase 6 e teste em campo obrigatório: o `connect` do jogo ao game
  server é capturado (log mostra o socket do GS) e a injeção funciona.

## 8. Fase 7 — Reorganizar `GameMemory` em sub-grupos (D5)

> Confirmação pendente: D5 prefere sub-namespace com rename (`Player::Level`); fallback de
> baixo risco é agrupar por seção de comentário mantendo os nomes.

- [ ] 8.1 Reorganizar `GameMemory.h` em sub-grupos `Object`/`Player`/`Globals`
  (sub-namespace ou seções, conforme decisão D5) sem mudar comportamento; verificar que o
  build passa e todos os callers compilam.
- [ ] 8.2 Commit da Fase 7 e teste em campo: monster tracker e ataque (que leem
  `GameMemory`) continuam funcionando.

## 9. Integração final (verificação que cruza grupos)

- [ ] 9.1 Teste em campo completo: ataque normal + skill + monster tracker + name-rewrite
  + comandos de debug; verificar que o log está íntegro e os bytes no wire são os mesmos
  da build pré-refactor.
- [ ] 9.2 Verificar que nenhum arquivo `.cpp` excede ~400 linhas e que `build.bat` compila
  sem warnings novos (`cl.exe /W3`); verificar por listagem de tamanhos.
- [ ] 9.3 Commit final da reorganização.

## Workflow follow-up

- Arquivar o change (`openspec archive`) após o teste em campo completo (9.1) passar.
- Como é refactor com `skip_specs: true`, o archive não sincroniza specs — apenas move o
  change para `openspec/changes/archive/`.
