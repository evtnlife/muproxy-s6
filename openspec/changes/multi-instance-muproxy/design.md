# Design

## Context

O byp (dll dentro do `main.exe`) fala com o `MuProxy2` por **dois canais TCP locais**:

| canal | porta (hoje) | quem escuta | quem conecta | código |
|---|---|---|---|---|
| CMD (comandos) | 44412 | `HookListenerService` | `CommandChannel` (byp) | `Config::TCP_PORT` |
| PKT (pacotes) | 44413 | `HookListenerService` | `PacketChannel` (byp) | `Config::PACKET_SNIFFER_TCP_PORT` |

As duas portas são **constantes de compilação** nos dois lados. Consequência: só existe um
par (44412/44413) por máquina, logo só um `MuProxy2`.

O byp roda **dentro** do processo do jogo (`opengl32.dll` → `LoadLibraryA("byp.dll")`), então
`GetCurrentProcessId()` do byp **é** o PID do `main.exe`. E o `MuProxy2` já cria o processo
do jogo (`CreateProcess` em `MainForm.cs`) e portanto conhece o PID antes do jogo começar a
falar com ele.

O loop de conexão do byp (`CommandChannel::WorkerThread`) já é de **retry**: se o socket
está `INVALID_SOCKET`, ele chama `DoConnect()` de novo (em loop, com `Sleep`). Esse retry é o
gancho natural para "re-resolver" a porta.

## Goals / Non-Goals

**Goals:**
- qualquer número de `MuProxy2` convivendo, cada um com o seu `main.exe`;
- nenhum arquivo do cliente (`.exe`/`.dll`) alterado;
- backward compatible: sem o arquivo de instância, tudo funciona como hoje (44412/44413);
- a janela diz qual personagem ela controla (título + header).

**Non-Goals:**
- uma instância do `MuProxy2` controlar dois jogos ao mesmo tempo;
- proxy do servidor de login compartilhado entre instâncias (hoje cada `MuProxy2` tem o seu
  `TcpLoopbackProxy`);
- mexer no `BlackrockMU.exe` (launcher) ou no `main.exe`.

## Decisions

### D1 — A porta é resolvida por um **arquivo de instância por PID**

`F:\blackrockmu\instances\<pid>.txt`:

```
cmd=44414
pkt=44415
```

- o byp lê `instances\<GetCurrentProcessId()>.txt` no `DllMain` (antes de inicializar os
  canais) e usa as portas de lá;
- sem arquivo → fallback `Config::TCP_PORT` / `Config::PACKET_SNIFFER_TCP_PORT`;
- a UI escreve o arquivo antes de fazer bind, e apaga quando o jogo morre.

**Alternativas descartadas:**
- *porta derivada do PID* (`44412 + pid % 100`): colisão entre PIDs, e a UI teria que
  adivinhar a porta de um jogo que ela não lançou;
- *porta fixa escolhida pelo usuário*: obriga o usuário a acertar a mão toda vez;
- *named pipe / memória compartilhada*: mais complexo e nãoNeeded — arquivo de texto já
  resolve e é depurável.

### D2 — O byp re-lê o arquivo a cada **falha** de conexão

O retry do `CommandChannel`/`PacketChannel` já existe; antes de cada `DoConnect()`, o byp
re-resolve a porta (lê o arquivo de novo). Consequência: se o jogo já estava rodando e a UI
só depois "adota" ele (escrevendo o arquivo), o byp passa a conectar sozinho — sem reiniciar
o jogo.

### D3 — A UI aloca portas por **teste de bind real**

Ao iniciar, a UI tenta `44412+N` / `44413+N` (N = 0,1,2,...) e para no primeiro par em que
consegue fazer bind dos dois sockets. Só então escreve o arquivo de instância. Isso elimina
conflito com outra instância já aberta (inclusive de outra sessão do Windows).

### D4 — A identidade do jogador vem do byp em **uma** mensagem

`PLAYER|name=..|level=..|map=..` (byp → UI, 1x quando muda):
- **level**: `F3 E0` (character info) — o `AutoPotion::ParseCharacterInfo` já faz o parse do
  level e hoje só loga;
- **name**: create-char (`NameRewriter`) e/ou `F3 15` (nome do personagem);
- **map**: pacote de troca de mapa (S→C, opcode `0x03` = `MuOpcodes::ServerToClient::MapChange`).

X/Y **não** vêm no `PLAYER`: a UI já tem a posição do jogador local pelo `MonsterService`
(`OnPlayerPositionChanged` → `toolTipPlayerCoord`). Mandar de novo seria duplicar fonte.

### D5 — O título só é montado quando há dados

Sem personagem identificado, o título é `MuProxy | (sem char)`. Com:
`MuProxy | HexDL | 214 | Lorencia | 120, 150`.

O mapa é mostrado pelo **nome** quando conhecido (`Lorencia`, `N DEV`, `Devil Square`…) e
pelo número quando não (`mapa 18`).

### D6 — A UI mantém a lista de instâncias

Cada entrada: PID do `main.exe`, portas, char/level/mapa (quando conhecidos), estado
(conectado/desconectado). A lista aparece num painel da aba "Custom Stuff" e permite
**adotar** um `main.exe` que já estava rodando (a UI acha o `main.exe` rodando, escreve o
arquivo de instância e passa a escutar as portas).

## Risks / Trade-offs

| risco | mitigação |
|---|---|
| o byp não acha o arquivo → conecta no 44412 e a UI "errada" recebe os dados | o log do byp diz **qual arquivo** ele leu (ou que não leu); a UI mostra a porta que ela alocou; o `PLAYER`/`POTION_STATE` traz o PID de origem? (opcional) |
| duas instâncias pegam a mesma porta | a UI testa o bind antes de escrever o arquivo (D3) |
| arquivo de instância obsoleto (jogo morreu) | a UI apaga o arquivo quando o processo termina; o byp ignora arquivo cujo PID não existe |
| o `main.exe` já aberto não tem arquivo → o byp fica no 44412 | o retry re-resolve (D2) → assim que a UI adotar, funciona |
| race na escrita do arquivo (UI escreve enquanto o byp lê) | escrita atômica: escrever em `.tmp` e `MoveFileEx` com replace |
| o log fica poluído com "connect falhou" a cada retry | o retry atual já loga; passa a logar **1 vez por resolução de porta**, não por tentativa |

## open questions

- o pacote que traz o **mapa** precisa ser identificado com uma captura (o opcode `0x03`
  está no enum, mas o layout do payload não foi conferido ainda) — mesma técnica do `F3 E0`;
- o **nome** do char: usar o create-char (que é o que o cliente envia) ou o `F3 15` (que é o
  que o servidor devolve)? Preferência: `F3 15` (é a verdade do servidor), create-char como
  fallback.
