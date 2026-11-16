# Tasks

> Cada grupo = 1 fase do Migration Plan (design.md) = 1 commit + `build.bat` + teste em
> campo. Verificação está embutida em cada task. Ordem por risco crescente.

## 1. Fase 0 — Limpeza de código morto e comentários

- [x] 1.1 Apagar a família PS.dll (`PsBypass`, `DllNotifyHook`, `MainPatcher`,
  `PsPacketHook`, `StartupHook`, `AsyncScanner`, `VPTracer` — .cpp + .h) e verificar que
  `build.bat` ainda compila (nenhum estava em `SOURCES`) e que não há `#include` restante.
- [x] 1.2 Apagar o rascunho `src/PacketSniffer.cpp.new` e verificar que nenhuma referência
  a `.cpp.new` existe no build ou nos scripts.
- [x] 1.3 Remover a fila de skill legado (`EnqueueSkillPacket`, `ProcessSkillQueue`,
  struct `SkillPacketEntry`, enum `SKILL_TYPE_*` de `PacketSniffer.h`) e verificar que o
  build passa e o skill continua indo por `InjectSkillC1` (teste em campo: cast de Force).
- [x] 1.4 Corrigir comentários obsoletos (ex.: `AttackModule.h` linha ~34 "0xEFF5 ...
  ataques normais E skill") e verificar por busca que não resta menção a 0xEFF5 aplicado
  a skill 0x19.
- [ ] 1.5 Commit da Fase 0 e smoke test em campo (jogo abre, log sem erro, ataque normal
  funciona).
  - [x] commit `e0945a2` (família PS.dll, `PacketSniffer.cpp.new`, fila de skill, comentários)
  - [ ] smoke test em campo (usuário): jogo abre, log sem erro, ataque normal funciona

## 2. Fase 1 — Extrair `MuCipher` (criptografia única)

- [x] 2.1 Criar `src/MuCipher.h` + `src/MuCipher.cpp` com `XorData`, `EncryptLayer1`/
  `DecryptLayer1`, `FullEncrypt`/`FullDecrypt`, `GetGameServerKeys`, `GetMuHeaderSize` e
  a tabela `g_XorFilter`; verificar que o arquivo compila isolado.
- [x] 2.2 Adicionar `src/MuCipher.cpp` à `SOURCES` do `build.bat` e verificar que o build
  gera `byp.dll`.
- [x] 2.3 Migrar os callers em `PacketSniffer.cpp` (e qualquer outro) para
  `MuCipher::*`, removendo as cópias locais; verificar que `XorData` só existe em
  `MuCipher` (busca por `g_XorFilter` retorna 1 resultado) e o build passa.
- [ ] 2.4 Commit da Fase 1 e teste em campo: ataque normal (0x11) e skill (0x19) produzem
  os mesmos bytes no wire que antes (conferir no log `InjectSkillC1: after XorData`).
  - [x] commit `c92afdc` (MuCipher criado, callers migrados, build OK)
  - [ ] teste em campo (usuário): ataque normal + skill com os mesmos bytes no wire

## 3. Fase 2 — Extrair `DebugTools` do `AttackModule`

- [x] 3.1 Criar `src/include/DebugTools.h` + `src/debug/DebugTools.cpp` (namespace
  `DebugTools`) e mover `DumpObjectManager`, `HexDumpSlot`, `ScanForValue`,
  `ScanSlotForValue`, `DirScan`, `SafeReadDword/Word/Byte/Float`; verificar que o build
  passa. (Os scans ficaram em `src/debug/DebugScans.cpp`, mesma namespace, para manter
  cada .cpp abaixo de ~400 linhas.)
- [x] 3.2 Re-registrar os comandos de debug (`CMD ATTACK DUMP`, `CMD ATTACK SCAN`, etc.)
  apontando para `DebugTools` via `PacketHandler`/`MessageRegistry`; verificar que os
  comandos respondem no MuProxy2. Tags registradas: `ATTACK_DUMP` (tag já usada pelo
  MuProxy2), `ATTACK_HEXDUMP`, `ATTACK_SCANVALUE`, `ATTACK_SCANSLOT`, `ATTACK_DIRSCAN`.
- [x] 3.3 Adicionar `src\debug\DebugTools.cpp` + `src\debug\DebugScans.cpp` à `SOURCES`
  do `build.bat` e verificar o build.
- [x] 3.4 Commit da Fase 2 e verificar que `AttackModule.cpp` ficou só com config,
  threads, build de pacotes e handlers (sem funções de dump/scan).
  - `AttackModule.cpp`: 1072 → 626 linhas; helpers de parse `key=value` movidos para o
    header compartilhado `src/include/CommandParams.h` (usado por AttackModule e DebugTools).

## 4. Fase 3 — Extrair `NameRewriter` e `PacketReassembler`

- [x] 4.1 Criar `src/include/NameRewriter.h` + `src/packets/NameRewriter.cpp` com
  `EncodeName`, `IsCreateCharPacket`, `g_NameKey`, `g_RewriteName`, `g_RewriteClass`;
  verificar que o build passa e o name-rewrite (HexDL) continua aplicado no pacote de
  create-char.
  - ATENÇÃO: o rewrite (HexDL) **não** está ligado a nenhum hook hoje (o código é
    material de RE, sem call-site). A extração preserva esse estado — nenhuma
    functionality nova foi ativada.
- [x] 4.2 Criar `src/include/PacketReassembler.h` + `src/packets/PacketReassembler.cpp`
  com os buffers por socket, `ReadMuPacketLength`, `Flush` (callback por pacote) e
  `Remove`; verificar que o build passa e o log de pacotes continua montando pacotes
  completos.
- [x] 4.3 Adicionar os dois .cpp à `SOURCES` do `build.bat` e verificar o build.
- [x] 4.4 Commit da Fase 3 e teste em campo: log de pacotes e name-rewrite íntegros.
  - commit `49f42fb` (build OK). Teste em campo do log de pacotes: pendente do usuário.

## 4b. Reorganização de pastas (pedido do usuário)

- [x] Nenhum `.cpp` solto na raiz de `src/` — um módulo por pasta:
  `core/`, `crypto/`, `net/`, `packets/`, `game/`, `attack/`, `debug/`.
  Headers continuam todos em `src/include/` (include path único `/Isrc\include`).
  Commit `eb7708d` (movimentação pura, sem mudança de código).

## 5. Fase 4 — `PacketInjector` + porta única de injeção (D1)

> Decisão D1 confirmada na aplicação: porta única `Inject(plaintext,len)` com
> `switch` por opcode (sem hierarquia de subclasses).

- [x] 5.1 Criar `src/include/PacketInjector.h` + `src/packets/PacketInjector.cpp` com
  `Inject(const uint8_t* plaintext, int len)` que aplica `MuCipher::XorData` uma vez e
  escolhe o pipeline pelo opcode (0x11 → Layer-1 C1 puro; 0x19 → SM+serial → C3 0D →
  Layer-1); verificar que o build passa.
- [x] 5.2 Migrar `AttackModule::SendAttack`/`SendSkillAttack` para chamar
  `PacketInjector::Inject`; verificar que o build passa.
- [x] 5.3 Remover `InjectC1Attack`/`InjectSkillC1`/`InjectToServer` antigos de
  `PacketSniffer.cpp`; o `PacketSniffer` virou o dono do socket do GS e registra o
  transporte (`SetSender`). Ficou a porta `InjectXorData` (dados JÁ na forma XorData,
  usada pelo comando SEND_PACKET — não aplica XorData de novo).
- [x] 5.4 Adicionar `src\packets\PacketInjector.cpp` à `SOURCES` do `build.bat` e
  verificar o build.
- [x] 5.5 Commit da Fase 4 (`406eb85`) e teste em campo: ataque normal e skill produzem
  os mesmos bytes no wire (conferir log `InjectSkillC1: after XorData`).
  - [ ] teste em campo (usuário): bytes idênticos ao pré-refactor

## 6. Fase 5 — Separar `CommandChannel` (44412) e `PacketChannel` (44413)

- [x] 6.1 Criar `src/include/CommandChannel.h` + `src/net/CommandChannel.cpp` (TCP 44412,
  comandos `CMD` — ex-`TcpClient`) e `src/include/PacketChannel.h` +
  `src/net/PacketChannel.cpp` (TCP 44413, log `PKT` + `SEND`); verificar que o build passa.
- [x] 6.2 Atualizar `dllmain.cpp`/`AttackModule`/`MonsterTracker`/`PacketHandler` para
  usar os dois canais; verificar que o build passa e não há socket duplicado.
  - `src/core/OriginalApi.{h,cpp}`: trampolines de send/connect, para os canais usarem as
    APIs ORIGINAIS (fora do hook) — o `CONNECT to 127.0.0.1:44412` não é mais logado a
    cada tentativa e o tráfego ASCII dos canais não entra mais no reassembler.
- [x] 6.3 Adicionar os .cpp à `SOURCES` do `build.bat` e verificar o build.
- [x] 6.4 Commit da Fase 5 (`f4b7b44`) e teste em campo: comandos chegam por 44412 e o
  log de pacotes sai por 44413 (conferir portas no log).
  - [ ] teste em campo (usuário)

## 7. Fase 6 — Extrair `WinsockHooks` (fase de maior risco)

- [x] 7.1 Criar `src/include/WinsockHooks.h` + `src/net/winsock/WinsockHooks.cpp` com
  trampolines, `AllocateTrampoline`, `InstallInlineHook`,
  `HookSend/Recv/WSASend/WSARecv/SendTo/RecvFrom/Connect/WSAConnect` e a completion
  routine overlapped; verificar que o build passa.
  - O código dos hooks foi conferido por diff contra a versão anterior: idêntico, exceto
    as trocas intencionais (`LogRaw` → observador, `PacketReassembler::Remove` →
    observador de socket fechado).
- [x] 7.2 Preservar a ordem síncrona de `Install()` em `DllMain` (`DLL_PROCESS_ATTACH`) —
  os hooks devem instalar antes de qualquer thread; verificado por inspeção: `dllmain.cpp`
  continua chamando `PacketSniffer::Install()` no `DLL_PROCESS_ATTACH`, e o
  `PacketChannel` (44413) só sobe depois dos hooks (para já usar o trampoline).
- [x] 7.3 Adicionar `src\net\winsock\WinsockHooks.cpp` à `SOURCES` do `build.bat` e
  verificar o build.
- [x] 7.4 Commit da Fase 6 (`6c16147`) e teste em campo obrigatório: o `connect` do jogo
  ao game server é capturado (log mostra o socket do GS) e a injeção funciona.
  - [ ] teste em campo (usuário)

## 8. Fase 7 — Reorganizar `GameMemory` em sub-grupos (D5)

> Decisão D5 aplicada: sub-namespace COM rename (`Player::Level`), pois só havia 33
> call-sites. Cada sub-namespace ganhou seu próprio header E seu próprio .cpp
> (pedido do usuário).

- [x] 8.1 Reorganizar `GameMemory` em `Globals`/`Object`/`Player`, cada um com header e
  .cpp próprios, sem mudar comportamento; build passa e todos os callers compilam:
  - `src/include/GameMemory/Globals.h` + `src/game/GameMemory/Globals.cpp`
  - `src/include/GameMemory/Object.h`  + `src/game/GameMemory/Object.cpp`
  - `src/include/GameMemory/Player.h`  + `src/game/GameMemory/Player.cpp`
  - `GameMemory.h`/`GameMemory.cpp` removidos; cada consumidor inclui só o que usa.
- [x] 8.2 Commit da Fase 7 (`3730e71`) e teste em campo: monster tracker e ataque (que
  leem `GameMemory`) continuam funcionando.
  - [ ] teste em campo (usuário)

## 9. Integração final (verificação que cruza grupos)

- [ ] 9.1 Teste em campo completo: ataque normal + skill + monster tracker + name-rewrite
  + comandos de debug; verificar que o log está íntegro e os bytes no wire são os mesmos
  da build pré-refactor.
- [x] 9.2 Verificar que nenhum arquivo `.cpp` excede ~400 linhas e que `build.bat` compila
  sem warnings novos (`cl.exe /W3`); verificado por listagem de tamanhos.
  - 27 arquivos .cpp; 24 abaixo de 400 linhas. Os 3 maiores são módulos coesos de
    responsabilidade única: `AttackModule` (628 — config + threads + build de pacotes +
    handlers, exatamente o que D4 define), `SimpleModulusCipher` (577 — cifra SM +
    leitura das chaves do jogo, acopladas pelo roundtrip de auto-teste) e
    `WinsockHooks` (486 — 8 hooks + trampolines).
  - Warnings: apenas os pré-existentes (`C4996` inet_ntoa/inet_addr e `C4530` /EHsc);
    nenhum warning novo.
- [x] 9.3 Commit final da reorganização (`5becc0a`).

## Workflow follow-up

- Arquivar o change (`openspec archive`) após o teste em campo completo (9.1) passar.
- Como é refactor com `skip_specs: true`, o archive não sincroniza specs — apenas move o
  change para `openspec/changes/archive/`.
