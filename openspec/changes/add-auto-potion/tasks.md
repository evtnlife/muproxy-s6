# Tasks

> Cada grupo = 1 fase = 1 commit + `build.bat` + teste em campo. A verificação está
> embutida em cada task.

## 1. Fase 1 — `MuCipher::UnXorData` + opcode 0x26 na porta de injeção

- [x] 1.1 Adicionar `MuCipher::UnXorData(char* data, int start, int end)` em
  `src/include/MuCipher.h` + `src/crypto/MuCipher.cpp` (mesma conta do `XorData`, ordem
  **decrescente**, igual ao `PacketManager::XorData` do servidor). Verificado com o
  exemplo `C1 07 11 00 1A 78 04` → `C1 07 11 EF ED B6 77` → volta ao plaintext.
- [x] 1.2 No `PacketInjector::Inject`, aceitar o opcode `0x26` (uso de item): Camada-2
  `[3,len)` + Layer-1, **sem** SimpleModulus. Validar `C1` + tamanho 6.
- [x] 1.3 Build (`build.bat`) e verificar que os opcodes 0x11/0x19 continuam idênticos
  (nenhum byte do wire muda — o `switch` só ganha um `case`).

## 2. Fase 2 — Módulo `AutoPotion`

- [x] 2.1 Criar `src/include/AutoPotion.h` (namespace `AutoPotion`): API pública
  (`Initialize`, `Shutdown`, `Start`, `Stop`, `IsRunning`, `OnItemUsePacket`,
  `GetConfig`/`SetConfig`) + config (`hpThreshold`, `mpThreshold`, `intervalMs`, slots).
- [x] 2.2 Criar `src/potion/AutoPotion.cpp`:
  - thread watcher (tick 100 ms) lendo `GameMemory::Player::Life()/MaxLife()` e
    `Mana()/MaxMana()`;
  - disparo: HP% < limiar → modelo de HP; MP% < limiar → modelo de MP;
  - intervalo mínimo entre usos (default 1000 ms);
  - envio via `PacketInjector::Inject` (modelo aprendido de 6 bytes) ou via slot
    configurado (`C1 06 26 <slot> FF 00`).
- [x] 2.3 Aprendizado do slot: `OnItemUsePacket` recebe o `C1 06 26` (forma Camada-2),
  decodifica com `UnXorData`, guarda o modelo e compara life/mana antes/depois para
  classificar como HP ou MP. Log de cada captura (bytes decodificados).
- [x] 2.4 Comandos `POTION_START`, `POTION_STOP`, `POTION_CONFIG`
  (`hp=`, `mp=`, `interval=`, `hpslot=`, `mpslot=`) e `POTION_STATUS`, registrados com
  `REGISTER_MESSAGE` no próprio `.cpp`; parse com `CommandParams`.

## 3. Fase 3 — Integração

- [x] 3.1 `PacketSniffer::OnReassembledPacket`: detectar `C1 06 26` na direção de saída e
  chamar `AutoPotion::OnItemUsePacket` (decodificação fica no módulo, não no sniffer).
- [x] 3.2 `dllmain.cpp`: `AutoPotion::Initialize()` no `SafeInitialize` e
  `AutoPotion::Shutdown()` no `DLL_PROCESS_DETACH` (antes de fechar os canais).
- [x] 3.3 `build.bat`: adicionar `src\potion\AutoPotion.cpp` às `SOURCES`.
- [x] 3.4 Build limpo e verificação: `byp.dll` gerado e deployado, **sem warnings novos**
  (só os pré-existentes C4996/C4530).

## 4. Fase 4 — MuProxy2 (C#)

- [x] 4.1 `Protocol/Messages.cs`: classes `POTION_START`, `POTION_STOP`, `POTION_CONFIG`
  e `POTION_STATUS` (com `[MessageTag]`).
- [x] 4.2 `Services/PotionService.cs`: serviço que envia as mensagens tipadas para o
  byp.dll (mesmo padrão do `AttackService`).
- [x] 4.3 UI (`MainForm.Designer.cs` + `MainForm.cs`): grupo "Auto Potion" na aba
  "Custom Stuff" — checkbox liga/desliga, campos de limiar de HP/MP, intervalo, botão
  "Aplicar" e botão "Status".
- [x] 4.4 `dotnet build -c Release` → 0 erros, 0 warnings.

## 5. Fase 5 — Validação em campo e documentação

- [ ] 5.1 Teste em campo (usuário):
  - beber uma potion de HP manualmente → conferir no log que o slot foi aprendido
    (`POTION: uso de item observado ...`) e que o life subiu;
  - idem com uma de MP;
  - `POTION_START` → levar dano → conferir que a potion foi bebida sozinha (life subiu
    sem o jogador clicar) e que o servidor **consumiu** o item (o contador de potions
    baixou);
  - `POTION_STOP` → confirmar que parou.
- [ ] 5.2 Se o servidor recusar o uso (anti-hack), aumentar `interval` e repetir.
- [x] 5.3 Documentado na skill do projeto (`.novamind/skills/blackrockmu`): novo §13
  "Auto potion", §12 (module map), §6 (tags novas) e §11 (linhas de log).
- [ ] 5.4 Commit final e marcar as tasks de campo do change `reorganize-byp-modules`
  (1.5, 2.4, 3.4, 4.4, 5.5, 6.4, 7.4, 8.2, 9.1) que ainda estão pendentes.

## 6. Fase 6 — Extras que entraram junto

- [x] 6.1 `MuOpcodes.h` reescrito como **enum tipada por direção**
  (`MuOpcodes::ClientToServer` / `MuOpcodes::ServerToClient` / `HeaderType` /
  `AttackType` / `Size`), porque o espaço de opcodes do MU é direcional: 0x26 = uso de
  item no C→S e update de life no S→C.
- [x] 6.2 `PacketInjector` reorganizado em **um pipeline por opcode**
  (`PipelineAttackNormal`, `PipelineTargetedSkill`, `PipelineItemUse`) com dispatch por
  `switch` — o módulo vai crescer e agora dá para adicionar opcode sem mexer no resto.
- [x] 6.3 Callers do `MuOpcodes` atualizados (`AttackModule.cpp`, `PacketSniffer.cpp`).
