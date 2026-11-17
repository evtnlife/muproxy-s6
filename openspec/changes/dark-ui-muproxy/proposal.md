# Proposal

## Why

A interface do `MuProxy2` é WinForms "crua" (gerada pelo Designer, tema padrão do Windows):
- **inputs sem hierarquia**: label colada no campo, sem alinhamento em grade, sem agrupamento
  visual — o "Intervalo (ms)" parece pertencer ao "MP abaixo de %";
- **40% da aba "Custom Stuff" vazio**: as três caixas (Monster List, Auto Potion, Attack
  Config) ficam todas no canto superior esquerdo de uma aba de 1045x687;
- **zero feedback de estado**: não há indicação visual de "proxy conectado", "byp
  conectado", "auto-potion ligado" — o usuário descobre olhando o log;
- **visual genérico**: cinza do Windows 10, sem identidade. O usuário pediu "padrão gamer".

Não é reescrita: é **organização + tema**. As funcionalidades continuam as mesmas.

## What Changes

- **Tema escuro** próprio (paleta definida no design), tipografia `Segoe UI`, controles flat
  (checkbox, radio, textbox, button, tab, listview, log) — sem depender de biblioteca
  externa.
- **Re-layout das abas**:
  - "Custom Stuff" vira um **dashboard em 3 colunas** (Monster List | Auto Potion | Attack
    Config) com a mesma altura e o espaço vazio preenchido por um painel de **header do
    personagem** + **lista de instâncias**;
  - "General" (Connection) e "Packets" reorganizados em campos com label **acima** do
    input, alinhados em grade;
  - "Logs" com fonte monoespaçada, cores por nível (INFO/WARN/ERRO) e o log de pacotes com
    destaque de direção.
- **Feedback de estado visível**: "LEDs" (verde/vermelho) para proxy conectado, byp
  conectado e auto-potion ligado.
- **Header do personagem** (nome, level, mapa, X/Y) no topo da aba — o mesmo dado do
  título da janela (change `multi-instance-muproxy`).

## Capabilities

### New Capabilities
- `muproxy-ui`: a interface do MuProxy2 tem tema, organização de inputs e feedback de
  estado consistentes.

### Modified Capabilities
(nenhuma)

## Impact

- **Código afetado**:
  - `byp/MuProxy2/MuProxy2/MainForm.Designer.cs` (layout, cores, owner-draw);
  - `byp/MuProxy2/MuProxy2/MainForm.cs` (aplicação do tema em runtime, header, LEDs);
  - novo `byp/MuProxy2/MuProxy2/UI/Theme.cs` (paleta + helpers de estilo);
  - novo `byp/MuProxy2/MuProxy2/UI/StatusLed.cs` (o LED de status).
- **Dependências**: **nenhuma nova** (sem NuGet, sem WebView2) — o tema é aplicado com
  `BackColor`/`ForeColor`/`FlatStyle`/`OwnerDraw`.
- **Protocolo**: nada muda.
- **Risco**: **baixo** (é cosmético), com um risco real: mexer no Designer pode quebrar o
  *layout* gerado — por isso a regra é **não reescrever o Designer à mão**: as mudanças de
  posição/tamanho saem do `Designer`, e o **tema** é aplicado em runtime (uma função
  `ApplyTheme()`), o que mantém o Designer legível e o diff pequeno.
- **Fora de escopo**: UI em WebView2 (HTML/CSS) — decisão adiada (ver design D5);
  ícones/imagens próprias; internacionalização.
