# Tasks

> Cada fase = build + teste em campo. A verificação está embutida em cada task.
> Ordem: o byp primeiro (Fase 1), a UI depois (Fase 2), a identidade do jogador por último
> (Fase 3) — porque o título depende do `PLAYER`.

## 1. Fase 1 — Porta em runtime no byp

- [ ] 1.1 `src/include/Config.h`: manter `TCP_PORT`/`PACKET_SNIFFER_TCP_PORT` como **fallback**
  e documentar que a porta real vem do arquivo de instância.
- [ ] 1.2 Criar `src/include/InstanceInfo.h` + `src/core/InstanceInfo.cpp`:
  - `Resolve(int* cmdPort, int* pktPort)` — lê `F:\blackrockmu\instances\<pid>.txt`
    (`GetCurrentProcessId()`), parseia `cmd=`/`pkt=`, e devolve `false` se não houver
    arquivo (aí o chamador usa o fallback);
  - `Describe()` → texto com o arquivo lido e as portas (para o log).
  - Registrar o `.cpp` no `SOURCES` do `build.bat` e verificar que o build passa.
- [ ] 1.3 `src/net/CommandChannel.cpp` + `src/net/PacketChannel.cpp`: a porta deixa de ser
  lida do `Config` na inicialização e passa a ser resolvida **a cada falha de conexão**
  (retry), com o fallback quando não há arquivo. Verificar que o build passa e que o log
  mostra a porta em uso.
- [ ] 1.4 `src/core/dllmain.cpp`: resolver a instância **antes** de `CommandChannel::Initialize`
  e `PacketChannel::Initialize`, e logar o resultado (`arquivo=... cmd=... pkt=...`).
- [ ] 1.5 Verificar que, **sem** arquivo de instância, o byp continua conectando em
  44412/44413 (compatibilidade) — teste de campo: o jogo abre e o `MuProxy2` mostra os
  monstros normalmente.

## 2. Fase 2 — UI: portas dinâmicas + arquivo de instância

- [ ] 2.1 `Services/HookListenerService.cs`: `Start()` recebe as portas (em vez de fixas
  44412/44413) e expõe as portas efetivamente bindadas.
- [ ] 2.2 Novo `Services/InstanceRegistry.cs`: alocar o **primeiro par de portas livres**
  (testando bind real), escrever `instances\<pid>.txt` de forma atômica (`.tmp` +
  `MoveFileEx`), e apagar o arquivo quando o processo do jogo terminar.
- [ ] 2.3 `MainForm.cs`: o botão "Launch Game" passa a (a) alocar as portas, (b) criar o
  processo, (c) escrever o arquivo de instância com o PID do `main.exe` criado, (d) iniciar
  o `HookListenerService` nessas portas.
- [ ] 2.4 Botão **"Adotar main.exe"**: lista os `main.exe` rodando, o usuário escolhe, a UI
  escreve o arquivo de instância com aquele PID e passa a escutar as portas alocadas (o byp
  passa a conectar sozinho pelo retry — D2).
- [ ] 2.5 Teste de campo: **dois** `MuProxy2` abertos, cada um com um `main.exe` — os dois
  jogos conectam, cada UI mostra **os dados do seu** jogo (monstros, potion, life/mana) e
  **não** os do outro.

## 3. Fase 3 — Identidade do jogador (`PLAYER`) e título da janela

- [ ] 3.1 byp: novo módulo `src/game/PlayerInfo.{h,cpp}` que junta as fontes do jogador local:
  - **level**: `F3 E0` (character info) — reaproveitar o parse que o `AutoPotion` já faz;
  - **name**: `F3 15` (nome do personagem) com o create-char como fallback;
  - **map**: pacote de troca de mapa (S→C, opcode `0x03`) — **layout a confirmar numa
    captura** antes de codificar;
  - e envia `PLAYER|name=..|level=..|map=..` para a UI quando algum dado muda.
- [ ] 3.2 `MuProxy2/Protocol/Messages.cs`: classe `PlayerMessage` (tag `PLAYER`) + parse;
  `DllCommandService` dispara o evento `OnPlayer`.
- [ ] 3.3 `MainForm.cs`: montar o título `MuProxy | <Nome> | <Level> | <Mapa> | <X>, <Y>`
  (X/Y do `MonsterService`); sem dados, `MuProxy | (sem char)`.
- [ ] 3.4 `MainForm.Designer.cs`: **header do personagem** no topo da aba "Custom Stuff"
  (nome, level, mapa, X/Y, com a mesma informação do título) + lista de instâncias
  (PID, portas, char, estado).
- [ ] 3.5 Teste de campo: o título da janela mostra o char/level/mapa/posição corretos e
  muda quando o char entra em outro mapa.

## 4. Fase 4 — Fechamento

- [ ] 4.1 Conferir que **uma** instância (sem arquivo) continua funcionando igual antes
  (44412/44413) — o caminho de fallback não pode ter quebrado nada.
- [ ] 4.2 Conferir o log do byp: ele diz qual arquivo de instância leu e em qual porta está
  conectado (diagnóstico de "a UI não está vendo o jogo").
- [ ] 4.3 `openspec/specs/muproxy-multi-instance/spec.md` e `openspec/specs/player-hud/spec.md`
  já foram criados como delta deste change (o `archive` aplica).
