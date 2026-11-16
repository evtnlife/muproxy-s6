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
| `POTION_CONFIG` | `hp=50|mp=30|interval=1000|hpslot=..|mpslot=..` | limiares (%) / intervalo (ms) / slots |
| `POTION_STATUS` | — | loga o estado (limiares, modelos, life/mana) |

- `hp`/`mp` = percentual que **dispara** a potion (default 50 / 30);
- `interval` = intervalo mínimo entre usos em ms (default **1000**, conservador
  por causa do anti-hack do servidor);
- `hpslot`/`mpslot` = slot flat do inventário (opcional; sem eles vale o modelo aprendido).

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
