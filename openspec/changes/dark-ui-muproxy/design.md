# Design

## Context

A UI é um `Form` WinForms único (`MainForm.cs` + `MainForm.Designer.cs`, ~1000 linhas de
Designer) com 4 abas:

| aba | conteúdo | coordenadas (Designer) |
|---|---|---|
| General | "Connection" | `groupBox1` (6,4) 620x170 |
| Custom Stuff | "Monster List", "Auto Potion", "Attack Config" | (7,7) 429x252 · (8,265) 425x200 · (442,7) 462x252 |
| Packets | "Protocol Filter", "Packet Sender" | (6,21) 586x86 · (622,21) 633x86 |
| Logs | lista de log + log de pacotes | — |

A aba "Custom Stuff" tem 1045x687 de área e usa só o canto superior esquerdo (o terço de
baixo à direita fica vazio). Tudo é tema padrão do Windows.

## Goals / Non-Goals

**Goals:**
- visual coeso e escuro, com identidade ("gamer") sem parecer genérico;
- inputs organizados: label acima do campo, alinhamento em grade, agrupamento por
 Cards, nada "colado";
- feedback de estado visível (proxy / byp / auto-potion / char);
- **zero dependência nova** e **zero mudança de comportamento**.

**Non-Goals:**
- reescrever a UI em WPF ou WebView2 (decisão adiada, D5);
- mudar o comportamento das features (só a apresentação);
- ícones/imagens próprias.

## Decisions

### D1 — Tema aplicado em **runtime**, não no Designer

O `Designer` fica só com estrutura (posição/tamanho/texto). O tema (cores, flat style,
fontes, cores do log) é aplicado por uma função `Theme.Apply(Form)` chamada no
`MainForm` logo após `InitializeComponent()`.

**Por quê:** o `Designer` é regenerável pelo Visual Studio — alterações de cor feitas à mão
se perdem na próxima edição do form. Aplicando o tema em runtime, o Designer continua
limpo e o diff do tema fica em **um** arquivo (`UI/Theme.cs`).

### D2 — Paleta

```
fundo do form      #12141A      painel/GroupBox   #1B1F27
card/label fundo   #20242E      texto             #E6E9EF
texto secundário   #8A93A6      acento            #FF9F43 (laranja)
ok                 #3DD68C      erro              #F85149
```

Laranja + cinza-azulado: escuro, "gamer", e o acento lê bem em texto pequeno.

### D3 — Hierarquia dos inputs

- **label acima** do campo (não ao lado) — padrão de ferramenta densa;
- grade: campos de mesmo tipo na mesma coluna, mesma largura (o `Auto Potion` tem 3 campos
  curtos → 3 colunas iguais; o `Connection` tem campos longos → 1 coluna);
- cada grupo vira um "card": `GroupBox` com borda na cor do painel, título em maiúscula
  pequena e espaçada;
- os fields numéricos com `TextBox` curto e alinhado à direita (números alinham melhor à
  direita).

### D4 — Feedback de estado: **LEDs** (label 8x8 arredondada com cor)

Três LEDs na barra superior do "Custom Stuff": `PROXY` (verde quando o
`TcpLoopbackProxy` está rodando), `BYP` (verde quando o byp conectou), `POTION` (verde
quando o auto-potion está ligado — a UI já recebe `POTION_STATE.active` 1x/s).

Sem ícone: só cor + rótulo + tooltip. É o feedback mais barato e mais legível.

### D5 — WebView2 fica **fora** (por enquanto)

O launcher `BlackrockMU.exe` já é WebView2, então a dependência existe no ambiente. Mas
traz uma camada nova (HTML ↔ C#) e um pipeline de messages novo. O restyle em WinForms
resolve 80% da percepção de "bonito" com risco baixo. Se depois de aplicadas as mudanças
ainda faltar "gamer", aí sim vale um change separado para WebView2.

### D6 — Header do personagem

Um painel no topo da aba "Custom Stuff" com nome/level/mapa/X/Y em fonte grande, alimentado
pela mensagem `PLAYER` (change `multi-instance-muproxy`). Antes do `PLAYER` existir, o
painel mostra "aguardando o byp".

## Impacto no código existente

- `MainForm.Designer.cs`: posições/tamanhos dos controles (reorganização) — e **só** isso;
- `MainForm.cs`: `ApplyTheme()` + os LEDs + o header;
- novos: `UI/Theme.cs`, `UI/StatusLed.cs`, `UI/PlayerHeader.cs`;
- `Services/`: nada muda (os dados do header já chegam por `OnPlayer`/`OnPotionState`).

## Risks / Trade-offs

| risco | mitigação |
|---|---|
| o Visual Studio regenerar o Designer e perder o re-layout | o tema é em runtime (D1) e o re-layout é mínimo (mover painéis, não recriar controles) |
| owner-draw do `TabControl`/`ListView` ficar inconsistente entre themes do Windows | usar só `FlatStyle`/cores (sem owner-draw) onde possível; owner-draw só no header das listas |
| contraste ruim em texto pequeno (8F) | contraste mínimo 4.5:1 verificado na paleta |
| diff grande no `Designer` | o re-layout é feito em etapas (uma aba por vez), com build+teste no meio |

## open questions

- manter os nomes das abas em inglês ("Custom Stuff") ou traduzir? → sugestão: traduzir
  ("Personagem", "Conexão", "Pacotes", "Logs") — mas é decisão do usuário;
- mostrar o log em abas separadas (log do sistema x log de pacotes) ou numa só com filtro?
  → hoje são dois `ListView` na aba "Logs"; unificar em **uma** lista com filtro é mais
  limpo, mas perde a separação visual.
