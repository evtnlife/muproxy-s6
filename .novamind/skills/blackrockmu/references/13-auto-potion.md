# 13. Auto potion (bypass)

> Módulo `AutoPotion` (`src/potion/AutoPotion.cpp`): bebe HP/MP automaticamente
> quando o percentual cai abaixo do limiar. Introduzido no change OpenSpec
> `add-auto-potion`.

## Protocolo do consumível (fonte: servidor de referência eMU `GameServer`)

```
C1 06 26 <SourceSlot> <TargetSlot> <type>        PMSG_ITEM_USE_RECV (opcode 0x26)
```

- `CItemManager::CGItemUseRecv` → `CObjectManager::CharacterUsePotion`:
  - `SourceSlot` tem que apontar para um **potion** no inventário do servidor
    (se não for, o pacote é ignorado e **nada é consumido** — sem punição);
  - `SourceSlot == TargetSlot` → o pacote é **rejeitado**;
  - `type` só importa para **Fruit** (`GET_ITEM(13,15)`): 0 = +ponto, ≠0 = −ponto;
  - potions: `GET_ITEM(14,0)` Apple, `14,1..3` Life, `14,4..6` Mana, `14,35..37` SD,
    `14,38..40` compostos, `14,70/71/133` elite (o efeito é % do MaxLife/MaxMana);
  - **anti-hack**: um segundo uso dentro de `CheckAutoPotionHackTolerance` ms é
    **rejeitado** (e a potion **não** é gasta) → o módulo respeita um intervalo mínimo;
  - o servidor **não** decide quando beber: o limiar é client-side.

## Camada-2 (XorData) vale para TODO C1
O `SocketManager` do servidor passa todo pacote recebido por
`PacketManager::ExtractPacket`, que faz `XorData(size-1, end)` — **descendente**,
cobrindo `[3, size)`. O cliente cifra **ascendente** em `[3, size)`
(`Main_EX603/Main/PacketManager.cpp`). Logo, **todo** pacote C1 do cliente para o
servidor é Camada-2 — inclusive o `C1 26` e o de movimento (`D4`), o que fecha a
questão aberta sobre movimento.

- `MuCipher::UnXorData(data, start, end)` = o inverso (mesma conta, ordem decrescente).
- O `XorData` **não** é auto-inverso in-place: ao decodificar, o byte anterior usado
  precisa ser o **cifrado** → a ordem tem que ser decrescente.

## Potions: como o servidor identifica o item (e onde está o nome)

O servidor decide o efeito pelo **ID do item** (`GET_ITEM(grupo,idx) = grupo*512 + idx`,
`MAX_ITEM_TYPE=512`) e **não** pelo slot:

| ID | item | efeito |
|----|------|--------|
| 7168..7171 | 14,0..3 (Apple, Small/Middle/Large Healing) | HP (% do MaxLife) |
| 7172..7174 | 14,4..6 (Small/Middle/Large Mana) | MP (% do MaxMana) |
| 7206..7208 | 14,38..40 (Small/Middle/Large Complex) | HP + SD |
| 7238 / 7239 | 14,70 / 14,71 (Elite Healing / Mana) | HP / MP |
| 6773..6776 | **13,117..120 ("Vitality Potion")** — exclusivo do BlackRock | HP |

### O ID dentro dos 12 bytes do ItemInfo (a pegadinha)

`CItemManager::ItemByteConvert` (GameServer/ItemManager.cpp) monta os bytes:

```
lpMsg[0] = m_Index & 0xFF;                    // índice bits 0..7
lpMsg[3] |= (m_Index & 256) >> 1;              // índice bit 8
lpMsg[5] |= (m_Index & GET_ITEM(15,0)) >> 5;   // grupo (bits 9..12 do índice) → bits 4..7 do byte5
```

Logo o decode é `id = info[0] | ((info[3]&0x02)<<7) | (((info[5]>>4)&0x0F)<<9)`.
**O grupo é o nibble ALTO do byte 5** — ler os bits 3..6 (o erro que o byp tinha) devolve o
grupo pela metade: o "Scroll of Twister" (15,7) virava "Siege Potion" (14,7) e as potions
de verdade passavam despercebidas.

### Nomes dos itens (para log/UI)

`F:\blackrockmu\Data\Local\Eng\ItemTooltip_eng.txt` é **texto puro**:
`grupo<TAB>indice<TAB>"Nome"<TAB>...` (931 itens). Serve para conferir o decode offline:
com o decode certo os IDs do inventário viram nomes coerentes ("Battle Scepter",
"Serpent Shield", "Jewel of Bless"...); com o decode errado saem coisas sem sentido.

### Pacotes do inventário (S→C)

| pacote | formato | uso |
|--------|---------|-----|
| `F3 10` (no C4 do login) | `[count][slot(1) + ItemInfo(12)]*count` | lista completa do inventário |
| `F3 14` | `[slot(1) + ItemInfo(12)]` | **cada mudança** no inventário (pegar/gastar/mover) |
| `C1 28` | `[slot(1) + flag(1)]` | item removido (potion gasta) |

O `F3 10` só chega no login — sem o `F3 14` o módulo fica cego para potions pegas
depois. `F3 14` e `C1 28` podem vir em C3 (SimpleModulus) → usar
`AutoPotion::OnServerPacket`, que já decodifica o corpo.

## Como o módulo descobre o slot do inventário ("learn & replay")
O inventário do **cliente** não está mapeado (`ObjectCharacter` tem só
`pEquipment[12]` + dinheiro) — o slot flat do servidor só o cliente sabe. Então:

1. o `PacketSniffer` vê o `C1 06 26` que o **jogador** mandou ao beber a potion
   (o sniffer só desfez a Layer-1 → o payload ainda está na forma Camada-2);
2. o `AutoPotion` decodifica com `UnXorData` e guarda os 6 bytes como **modelo**;
3. compara o life/mana **antes e depois**: se o life subiu → modelo de **HP**;
   se o mana subiu → modelo de **MP**;
4. ao beber, o módulo **replaya o modelo verbatim** (nada é montado por palpite —
   inclusive o `TargetSlot` e o `type`).

Quem manda no slot pode ser o modelo aprendido **ou** a config
(`POTION_CONFIG|hpslot=25|mpslot=26` → monta `C1 06 26 <slot> FF 00`).

> Nossas próprias injeções **não** voltam pelo hook (o `GameServerConnection` usa o
> `send` original), então o auto-potion nunca confunde o uso dele com o do jogador.

## Comandos (MessageRegistry, 44412)
| Tag | Payload | Efeito |
|-----|---------|--------|
| `POTION_START` | — | liga o auto-potion |
| `POTION_STOP` | — | desliga |
| `POTION_CONFIG` | `hp=50\|mp=30\|interval=1000\|hpslot=..\|mpslot=..` | limiares (%) / intervalo (ms) / slots |
| `POTION_APPLY` | igual ao CONFIG | **aplica a config E liga** numa mensagem só (idempotente) |
| `POTION_STATUS` | — | loga o estado (limiares, modelos, life/mana) |

- `hp`/`mp` = percentual que **dispara** a potion (default 50 / 30);
- `interval` = intervalo mínimo entre usos em ms (default **1000**, conservador
  por causa do anti-hack do servidor);
- `hpslot`/`mpslot` = slot flat do inventário (opcional; sem eles vale o modelo aprendido).

### Regra de estado (UI ↔ módulo)

O checkbox da UI **é** o estado do módulo: marcado = ligado. Não existe botão
"Aplicar" — a UI manda `POTION_APPLY` (debounce de 400 ms) em qualquer mudança e
`POTION_STOP` quando desmarca.

⚠️ O `POTION_APPLY` do byp **não descarta** mensagem nenhuma (o módulo tem que
aceitar uma reconfig legitimate logo depois de outra). E o `POTION_STATE` (1x/s,
com `active=0|1`) é a fonte de verdade: se ele disser `active=0` com o checkbox
marcado, é porque o byp reiniciou — a UI reaplica.

O byp.dll é recarregado junto com o jogo: ao reconectar no canal 44412 o módulo
volta do zero (config default, desligado). Por isso a UI reaplica o estado
**na conexão**, não só no clique.

## Log (tag `POTION`)
```
[POTION] Inicializado (hp<50% mp<30% intervalo=1000ms)
[POTION] uso de item observado (cliente): slot=25 target=0xFF type=0x00
[POTION] modelo de HP aprendido (slot=25)
[POTION] potion de HP disparada em 38% (slot=25) — uso injetado
[POTION] estado: LIGADO | hp<50% mp<30% intervalo=1000ms | modelo HP=ok (slot 25) | ...
```
- `uso de item observado` = o **jogador** bebeu a potion (aprendizado);
- `modelo de HP/MP aprendido` = o efeito confirmou o tipo;
- `potion de ... disparada em N%` = o auto-potion injetou o uso.

## Teste em campo (checklist)
1. Beber uma potion de HP **manualmente** → o log deve mostrar `uso de item observado`
   e `modelo de HP aprendido`.
2. Idem com uma de MP → `modelo de MP aprendido`.
3. `POTION_START` (ou o checkbox na UI) → levar dano → a potion é bebida sozinha
   (o life sobe sem clicar) **e o servidor consome o item** (o contador de potions cai).
4. `POTION_STOP` → para.
5. Se o uso for **rejeitado** (anti-hack): o life não sobe e o log mostra o pacote
   enviado → aumentar `interval`.

## Armadilhas
- O `gg_bypass2.log` **não** loga o `C1 26` por pacote (o `PacketInjector` loga só o
  uso **injetado**; o que o jogador manda aparece só na linha `POTION`).
- O slot flat do servidor **não** é a posição na grade do inventário do cliente:
  não chute — use o aprendizado (ou descubra pelo log `uso de item observado`).
