# Tasks

> Cada fase = build + verificação visual. O tema é aplicado em **runtime** (o `Designer`
> fica só com estrutura), então dá para reverter uma etapa sem desfazer layout.

## 1. Fase 1 — Base do tema

- [ ] 1.1 Criar `MuProxy2/UI/Theme.cs`: paleta (cores do design D2), fonte base
  (`Segoe UI 9`), e helpers (`FlatButton`, `DarkTextBox`, `DarkCheckBox`, `DarkRadio`,
  `DarkListView`, `DarkTabControl`).
- [ ] 1.2 `MainForm`: chamar `Theme.Apply(this)` no final do construtor e conferir a
  janela (build + screenshot).
- [ ] 1.3 Aplicar o tema aos `ListView` (log e pacotes): fundo escuro, texto claro,
  cabeçalho sem seleção azul, `FullRowSelect`/`GridLines` conforme o visual antigo.
- [ ] 1.4 Verificar que **nenhuma funcionalidade quebrou** (o log continua rolando, as
  listas continuam atualizando, os checkboxes continuam funcionando).

## 2. Fase 2 — Organization dos inputs

- [ ] 2.1 Aba "General" (Connection): label **acima** dos campos, alinhamento em grade
  (mesma largura), agrupar "servidor" e "cliente" em cards.
- [ ] 2.2 Aba "Custom Stuff" → **dashboard de 3 colunas**: Monster List | Auto Potion |
  Attack Config, todas com a mesma altura; o espaço vazio de baixo à direita fica para o
  header do personagem (Fase 4) e a lista de instâncias.
- [ ] 2.3 Aba "Packets": Protocol Filter e Packet Sender com labels acima e campos
  alinhados; a área de hex fica com fonte monoespaçada.
- [ ] 2.4 Campos numéricos alinhados à direita e com largura fixa.
- [ ] 2.5 Verificação visual de cada aba (screenshot) + build limpo.

## 3. Fase 3 — Feedback de estado (LEDs)

- [ ] 3.1 Criar `MuProxy2/UI/StatusLed.cs`: label 8x8 com cor (verde `#3DD68C` /
  vermelho `#F85149` / cinza `#8A93A6`) + rótulo + tooltip.
- [ ] 3.2 Barra de status no topo da aba "Custom Stuff" com 3 LEDs: `PROXY`, `BYP`,
  `POTION`.
- [ ] 3.3 Ligar os LEDs: proxy (estado do `TcpLoopbackProxy`), byp (`OnDllConnected` /
  `OnDllDisconnected`), potion (`POTION_STATE.active`).
- [ ] 3.4 Teste de campo: derrubar o jogo → o LED `BYP` fica vermelho; religar → verde.

## 4. Fase 4 — Header do personagem

- [ ] 4.1 Criar `MuProxy2/UI/PlayerHeader.cs`: painel com nome, level, mapa e X/Y em
  fonte maior, alimentado pela mensagem `PLAYER` (change `multi-instance-muproxy`).
- [ ] 4.2 Enquanto não houver `PLAYER`, o painel mostra "aguardando o byp".
- [ ] 4.3 X/Y vem do `MonsterService` (já existe) → atualizar em runtime, sem recriar
  controles.
- [ ] 4.4 Verificação visual final de todas as abas.

## 5. Fase 5 — Polimento

- [ ] 5.1 Log com fonte monoespaçada e cor por nível (INFO / WARN / ERRO / destaque do
  `POTION`).
- [ ] 5.2 Tooltips nos controles de baixo valor de uso (LEDs, campos).
- [ ] 5.3 Remover/limpar controles que sobraram (o "Status" do auto-potion já foi removido;
  conferir se sobrou algo morto no Designer).
- [ ] 5.4 Checagem final: `git diff` só com o esperado, build sem warning, e o software
  com o mesmo comportamento de antes (auto-potion, ataque, monstros, log de pacotes).
