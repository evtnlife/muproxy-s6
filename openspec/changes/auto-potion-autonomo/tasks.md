# Tasks

> Cada grupo = 1 fase = build + teste em campo. A verificacao esta embutida em cada task.

## 1. Fase 1 — Decifrar C3/C4 no S→C (a raiz de tudo)

> **Diagnóstico de campo (06:07)**: a decifra S→C **já funcionava** para C3 — o log de
> diagnóstico mostra `C3-SM decodificado len=167 : 00 F3 03 25 34 07 ...` (o `F3 03`, o
> character info). O que faltava era o **C4**: o `F3 10` (lista de itens) vem num
> **C4 de 784 bytes** (≈60 itens × 13 bytes) e o `C4` tem header de **3 bytes**, que o
> `TryDecodeSmBody` nunca tratava.

- [x] 1.1 `AutoPotion::TryDecodeSmBody`: tratar **C4** (header de 3 bytes: C4, sizeHi, sizeLo)
  alem do C3 (2 bytes). Validar o tamanho minimo (`SM_ENCRYPTED_SIZE`).
- [x] 1.2 Buffer de decifra ampliado (512 → 2048): o C4 de 784 bytes decifra 568.
- [x] 1.3 `SimpleModulusCipher`: o `DecryptReceive` ja valida o **checksum por bloco** (o
  `DecryptBlockWith` devolve -1 quando falha) e testa todas as chaves candidatas — validado
  offline: o C3 do login decodifica com a chave de **recepcao**
  (`Mod=[11E6E 1ADA5 1821B 29C32] Dec=[4673 7684 607D 2B85] Xor=[F234 FB99 8A2E FC57]`).
- [x] 1.4 Teste de campo: log esperado `C4-SM decodificado` + `lista de itens (F3 10): N item(ns)`.
   **OK (06:35:53)**: `C4-SM decodificado len=563` + `lista de itens (F3 10): 43 item(ns)`.

## 2. Fase 2 — Inventario: `F3 10` → `slot → item ID`

- [x] 2.1 `AutoPotion::ParseItemList`: o layout do servidor e `[count][slot(1) + ItemInfo(12)]*count`
  (`CItemManager::GCItemListSend` monta o pacote a mao) — o parser existente ja esta correto;
  faltava o pacote chegar.
  ⚠️ **`DecodeItemId` estava ERRADO**: o grupo do item esta nos **bits 4..7** do byte 5
  (`lpMsg[5] |= (m_Index & GET_ITEM(15,0)) >> 5`, `CItemManager::ItemByteConvert`), e nao
  nos bits 3..6. Corrigido na Fase 7.
- [x] 2.2 Teste de campo: o log tem de mostrar `lista de itens (F3 10): N item(ns)` + os slots.
  **OK**: 43 item(ns), slots 0..51 (com buracos) — o layout do parser bate.
- [x] 2.3 Teste de campo: os IDs tem de bater com o inventario do jogo (ex.: `14,4` = 7172).
  **OK**: com o decode certo, os 21 primeiros itens viram nomes coerentes com a tabela do
  cliente (`Data\Local\Eng\ItemTooltip_eng.txt`): Battle Scepter, Serpent Shield,
  Light Plate Mask/Armor, Scale Pants, Light Plate Gloves/Boots, Pendant of Fire,
  Ring of Earth, Ring of Ice, Jewel of Chaos Bundle, Box of Luck, Jewel of Bless,
  Scroll of Twister, Scroll of Aqua Beam, Lucky Coin, Imp, Blood Bone.

## 3. Fase 3 — Escolha automatica do slot (sem beber potion antes)

- [x] 3.1 Classificar os slots por ID — tabela em `AutoPotion::POTIONS` (14,0..6 / 14,38..40 /
  14,70,71 / **13,117..120 = "Vitality Potion"**, exclusivo deste server) em vez de faixa de IDs.
- [x] 3.2 `BuildItemUsePacket`: prioridade = `hpslot`/`mpslot` manual → **slot automatico do
  inventario** → `hpitem`/`mpitem` → modelo aprendido (fallback).
- [ ] 3.3 Teste de campo: **sem nunca ter bebido a potion**, o log tem de mostrar
  `potion de HP disparada em X% (slot=N)` com N vindo do inventario.

## 4. Fase 4 — Troca automatica quando a potion acaba

- [x] 4.1 Detectar **uso sem efeito**: `VerifyAutoUse()` compara o life/mana 900 ms apos o
  uso; se nao subiu, o slot e marcado como suspecto.
- [x] 4.2 Trocar para o proximo slot do mesmo tipo (round-robin sobre a lista do inventario)
  e registrar no log.
- [x] 4.3 Sem mais slots do tipo: logar `potion de HP no slot N nao teve efeito e nao ha outro
  slot de HP` e **parar de injetar** nesse slot (hoje ele continua injetando e o log mente).
- [ ] 4.4 Teste de campo: acabar as potions → o log tem de mostrar a troca
  (`slot 47 sem efeito → usando slot 12`).

## 5. Fase 5 — Life/mana maximo (robustez)

- [ ] 5.1 Manter o **ultimo max conhecido** quando um pacote nao traz max (o `F3 E1` e um
  snapshot de 80s; o max so muda em level up).
- [ ] 5.2 `F3 E1` (calc) → `ViewMaxHP`/`ViewMaxMP` (DWORD LE, offsets 4 e 12 do payload).
- [ ] 5.3 `C1 26`/`C1 27` com `type = 0xFE` → max (esse servidor nao manda, mas o cliente
  aceita — o codigo ja trata).
- [ ] 5.4 Teste de campo: o log `fontes:` tem de mostrar `max=214/219` de forma estavel.

## 6. Fase 6 — UI (MuProxy2)

- [x] 6.1 Mostrar no painel o slot de HP/MP em uso e a origem dele (inventario/aprendido).
  Nova mensagem **byp → UI** `POTION_STATE|active=..|hp=..|hpSrc=..|mp=..|mpSrc=..|items=..|`
  `life=..|lifeMax=..|mana=..|manaMax=..|warn=..` (enviada 1x/s pelo canal de comandos).
- [x] 6.2 Mostrar o aviso `sem potion de HP no inventario` quando nao houver slot
  (o `warn` da mensagem; a label fica em vermelho).
- [x] 6.3 Nao mexer nos limiares a cada tecla (ja resolvido: so o botao "Aplicar" envia) e
  **debounce** do botao (o repeat de teclado do Windows disparava 6-8 mensagens).
- [x] 6.4 Painel `lblPotionState` no grupo "Auto Potion": life/mana com %, slot em uso,
  origem (inv/cfg/id/apr) e o aviso.
- [x] 6.5 **Tirar da UI os campos "Item de HP/MP (hex)" e "Slot de HP/MP"**: o modulo
  descobre a potion sozinho (F3 10 no login + F3 14 a cada mudanca) e mostra o que achou
  (slot + nome da potion) no painel.
- [x] 6.6 **Tirar o botao "Aplicar"**: o estado do checkbox É o estado do modulo.
  Qualquer mudanca (checkbox, limiar, intervalo) agenda UM `POTION_APPLY` (debounce de
  400 ms na UI); desmarcar manda `POTION_STOP`.
- [x] 6.7 **Sincronizar UI ↔ byp**: (a) quando o byp.dll reconecta (o modulo volta do
  zero com o jogo) a UI reaplica o estado; (b) se o byp reportar `active=0` com o
  checkbox marcado, a UI reaplica (backoff de 10 s) — sem isso o usuario tinha que
  desmarcar/marcar para o byp "acordar".
- [x] 6.8 **byp: `POTION_APPLY` nao descarta mais mensagem** — o debounce de 1 s que
  existia (pensado para o repeat do teclado do botao) engolia uma configuracao legitima
  (mexer no limiar 2x em 1 s): a UI mostrava um valor e o modulo usava outro.

## 7. Fase 7 — O ID do item estava pela metade (a causa de "nao achou os pot")

> **Diagnostico de campo (06:35)**: o log mostrava `pocoes no inventario: 0 slot(s) de HP,
> 0 de MP` com 43 itens no inventario. Duas causas, as duas corrigidas aqui.

- [x] 7.1 `DecodeItemId`: o **grupo** do item esta nos **bits 4..7** do byte 5
  (`CItemManager::ItemByteConvert`: `lpMsg[5] |= (m_Index & GET_ITEM(15,0)) >> 5`), e nao
  nos bits 3..6 — o grupo saia pela metade (o "Scroll of Twister" virava "Siege Potion" e as
  potions verdadeiras passavam despercebidas).
- [x] 7.2 Tabela de potions por **ID + nome** (`AutoPotion::POTIONS`): 14,0..6 / 14,38..40 /
  14,70,71 / **13,117..120 "Vitality Potion"** (exclusivo deste server).
- [x] 7.3 **F3 14** (`PMSG_ITEM_MODIFY_SEND`): o servidor avisa cada mudanca do inventario —
  e por ali que uma potion pega no jogo entra na tabela `slot → ID` (o F3 10 so vem no login).
- [x] 7.4 **C1 28** (`PMSG_ITEM_DELETE_SEND`): quando a potion e gasta, o slot sai da tabela
  (senao o modulo fica usando um slot vazio).
- [x] 7.5 `POTION_STATE` com `hpName`/`mpName` — o painel da UI mostra o **nome** da potion
  em uso (ex.: `slot 30 (Healing Potion)`).
- [x] 7.6 Log do item com grupo/indice e nome: `slot=13 item=0x1C0B (grupo 14, idx 11) Box of Luck`.
- [x] 7.7 Teste de campo: o log tem de mostrar `pocoes no inventario: N slot(s) de HP (nome),
  M de MP (nome)` assim que a potion entrar no inventario (F3 14).
  **OK (07:33:04)**: `lista de itens (F3 10): 42 item(ns)` +
  `pocoes no inventario: 4 slot(s) de HP (Large Healing Potion), 1 de MP (Large Mana Potion)`
  (slots 22/29/37/47 = 14,3 e slot 30 = 14,6).
- [x] 7.8 Teste de campo: o personagem **tem** potion na mochila — na sessao das 06:35
  (43 itens) realmente nao tinha nenhuma (armaduras, escudos, joias, scrolls; os slots
  30 e 47 que o jogador clicou eram `Skull Shield` e `Elven Shield`); as potions foram
  compradas/pegas depois e apareceram na sessao das 07:33.
- [x] 7.9 Teste de campo: o auto-potion **liga sozinho** ao subir o jogo (07:32:46
  `config:` + `auto-potion LIGADO` na conexao do byp — sem clicar em nada).
