# Proposal

## Why

O `MuProxy2` só pode rodar **uma instância**: as portas do canal de comando (44412) e do
canal de pacotes (44413) são constantes de compilação (`byp/gg_bypass2/src/include/Config.h:24`
e `:28`) e o `HookListenerService` faz bind nelas (`MuProxy2/Services/HookListenerService.cs:38`
e `:42`). A segunda instância falha no bind e a segunda conta não tem controle nenhum.

Para jogar com mais de um personagem ao mesmo tempo (o caso do `AutoLogin` do `Settings.ini`,
que já traz `User`/`Password`), o usuário precisa de **um `MuProxy2` por `main.exe`** — e
cada janela precisa dizer **qual** personagem ela está controlando, senão são 3 janelas
iguais no taskbar.

Hoje o `MuProxy2` já sabe o PID do `main.exe` que ele lançou (`CreateProcess` em
`MainForm.cs`, botão "Launch Game"), e o byp (que roda **dentro** do processo do jogo) sabe o
seu próprio PID — ou seja, os dois lados já têm a chave que falta para se casar.

## What Changes

- **Portas por instância**: a porta deixa de ser constante e passa a ser resolvida em runtime.
  - byp: ao inicializar, lê `F:\blackrockmu\instances\<pid>.txt` (o PID do byp **é** o PID do
    `main.exe`) e usa as portas que estão nele; sem arquivo, cai no padrão 44412/44413
    (compatibilidade com asetup atual);
  - UI: escolhe o **primeiro par de portas livres** (44412+N / 44413+N), escreve o arquivo de
    instância e faz bind nele.
- **Adotar um `main.exe` que já está rodando**: o byp re-lê o arquivo de instância a cada
  falha de conexão (o loop de retry já existe) → a UI pode "adoptar" um jogo aberto depois.
- **Título da janela com a identidade do personagem**:
  `MuProxy | <Nome> | <Level> | <Mapa> | <X>, <Y>`.
  - byp → UI: nova mensagem `PLAYER|name=..|level=..|map=..` (o byp já vê o level no `F3 E0`
    e o nome no create-char / `F3 15`; o mapa vem do pacote de troca de mapa, opcode `0x03`);
  - a UI já tem X/Y do `MonsterService` → monta o título e um header do personagem.
- **Lista de instâncias na UI**: cada `MuProxy2` mostra a(s) janela(s) que ele cuida
  (PID, portas, char, status), porque uma instância pode passar a ser a "mestre" de mais de
  um jogo.

## Capabilities

### New Capabilities
- `muproxy-multi-instance`: vários `MuProxy2` convivendo, cada um ligado a um `main.exe`,
  com portas e dados isolados.
- `player-hud`: identidade do jogador local (nome, level, mapa, posição) exposta pelo byp e
  consumida pela UI (título da janela + header).

### Modified Capabilities
(nenhuma)

## Impact

- **Código afetado**:
  - `byp/gg_bypass2/src/include/Config.h` (constantes viram fallback);
  - `byp/gg_bypass2/src/net/CommandChannel.cpp` e `src/net/PacketChannel.cpp` (porta em runtime);
  - `byp/gg_bypass2/src/core/dllmain.cpp` (resolver a instância antes de inicializar os canais);
  - `byp/gg_bypass2/src/game/` (novo módulo `PlayerInfo` — nome/level/mapa → `PLAYER`);
  - `byp/MuProxy2/MuProxy2/Services/HookListenerService.cs` (portas dinâmicas);
  - `byp/MuProxy2/MuProxy2/MainForm.cs` + `MainForm.Designer.cs` (instância, título, header).
- **Protocolo**: nova tag byp→UI `PLAYER`; o formato dos canais não muda (só a porta).
- **Disco**: uma pasta nova `instances\` na raiz do jogo (arquivos de texto, ~40 bytes cada).
- **Risco**: **baixo–médio**. O ponto sensível é a resolução de porta (se o byp não achar o
  arquivo, ele volta ao padrão e a UI fica sem o jogo — por isso o log tem de dizer qual
  arquivo ele leu e qual porta ele usou).
- **Fora de escopo**: uma instância controlar **dois** jogos ao mesmo tempo (o título
  mostraria um char por vez); proxy do servidor de login (o `TcpLoopbackProxy` continua
  apontando para o mesmo host/porta de todos).
