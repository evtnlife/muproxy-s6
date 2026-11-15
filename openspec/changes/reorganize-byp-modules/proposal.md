# Proposal

## Why

O byp.dll funciona (ataque normal 0x11 e skill 0x19 validados em campo), mas o código
acumulou desorganização que trava a manutenção: `PacketSniffer.cpp` é um "god file" de
1577 linhas com 6 responsabilidades misturadas (hooks winsock, reassembly, criptografia,
dois canais TCP, name-rewrite, injeção), `AttackModule.cpp` carrega ~400 linhas de
ferramenta de debug/RE que não pertencem ao módulo, e existe uma família inteira de
arquivos mortos (PS.dll) que nem compila. Cada pacote novo de ataque (movimento, etc.)
exige entender esse emaranhado antes de tocar em algo.

Agora que o pipeline está validado, é o momento de reorganizar sem risco: o comportamento
observável (pacotes no wire, comandos do MuProxy2, injeção) permanece idêntico.

## What Changes

- **Remover código morto** (sem impacto no build atual):
  - Família PS.dll: `PsBypass.cpp/.h`, `DllNotifyHook.cpp/.h`, `MainPatcher.cpp/.h`,
    `PsPacketHook.cpp/.h`, `StartupHook.cpp/.h`, `AsyncScanner.cpp/.h`, `VPTracer.cpp/.h`
    (nenhum está na lista `SOURCES` do `build.bat`).
  - Rascunho `PacketSniffer.cpp.new` (fora do build).
  - Fila de skill legado: `EnqueueSkillPacket`, `ProcessSkillQueue`, enum `SKILL_TYPE_*`
    (o skill agora vai direto por `InjectSkillC1`; a fila é resíduo do caminho antigo).
- **Extrair a criptografia** num módulo único `MuCipher` (Layer-1, `XorData`,
  `GetGameServerKeys`, `GetMuHeaderSize`). `XorData` sai de `static` e vira a ÚNICA porta
  de XOR do projeto, visível a qualquer módulo.
- **Desmontar o god file `PacketSniffer.cpp`** em módulos com uma responsabilidade cada:
  `WinsockHooks` (trampolines + hooks), `PacketReassembler` (stream → pacotes MU),
  `CommandChannel` (TCP 44412) / `PacketChannel` (TCP 44413), `PacketInjector`
  (injeção + pipelines), `NameRewriter` (rewrite HexDL).
- **Unificar a porta de injeção**: hoje há 3 funções com 3 contratos
  (`InjectToServer` recebe forma XorData; `InjectC1Attack`/`InjectSkillC1` recebem
  plaintext). Vira UMA porta `Inject(plaintext, len)` que decide o pipeline pelo opcode
  (0x11 → C1 puro; 0x19 → XorData → SM+serial → C3 0D). O XorData é aplicado uma vez,
  num único lugar, para todos os opcodes.
- **Limpar `AttackModule.cpp`**: mover ferramentas de debug/RE
  (`DumpObjectManager`, `HexDumpSlot`, `ScanForValue`, `DirScan`, `SafeRead*`) para
  `DebugTools` (ou remover).
- **Reorganizar `GameMemory`** em sub-grupos (`Object`, `Player`, `Globals`) sem mudar
  nomes públicos.
- **Corrigir comentários obsoletos** (ex.: `AttackModule.h` ainda cita XOR 0xEFF5 para
  skill, errado desde o fix do 0x19).

Nenhuma mudança de comportamento: os bytes no wire, os comandos aceitos do MuProxy2 e o
fluxo de injeção são os mesmos antes e depois.

## Capabilities

### New Capabilities
<!-- Refactor pura: nenhuma nova capability de comportamento. -->
(nenhuma)

### Modified Capabilities
<!-- Refactor pura: nenhum requisito de comportamento muda. -->
(nenhuma)

> Este change é uma **refactor pura** (reorganização + remoção de código morto), sem
> mudança de comportamento observável. Por isso `.openspec.yaml` define
> `skip_specs: true` — não há requisito de spec a criar ou alterar.

## Impact

- **Código afetado**: `byp/gg_bypass2/src/**` (14 .cpp no build + headers). Arquivos
  mortos removidos. `build.bat` atualizado (novos .cpp em `SOURCES`, mortos fora).
- **APIs internas**: namespaces `PacketSniffer` e `AttackModule` são reorganizados;
  funções públicas usadas por `dllmain.cpp` (`Install`, `Uninstall`, `Initialize`,
  `Shutdown`) mantêm assinatura.
- **Dependências**: nenhuma nova. `SimpleModulusCipher` e `GameMemory` permanecem como
  folhas estáveis.
- **Risco**: baixo-médio. A fase de maior risco é a extração dos hooks winsock
  (ordem de instalação em `DllMain` é crítica para capturar o connect do jogo). Cada
  fase é um commit com build + teste em campo.
- **Fora de escopo**: offsets em `GameAddresses.h` (validados por RE), lógica de
  criptografia (só é movida, não alterada), MuProxy2 (C#) e o servidor MuEmu.
