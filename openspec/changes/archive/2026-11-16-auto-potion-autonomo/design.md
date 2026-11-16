# Design

## Context

O modulo `AutoPotion` decide sozinho (thread watcher, tick 100 ms) quando injetar
`C1 06 26 <slot> 00 00` no socket do game server. Hoje ele depende de duas coisas que
**nao chegam no byp**:

1. **o slot da potion** — discovery "learn & replay" (o jogador bebe 1x de cada tipo);
2. **o life/mana maximo** (para calcular o %) — vem do `F3 E1` (calc) ou de um `C1 26`
   com `type = 0xFE`. O servidor BlackRock nao manda `0xFE`; o `F3 E1` chega as vezes.

Este design resolve os dois a partir da mesma raiz: **decifrar os pacotes C3/C4 do S→C**.

## Protocolo (capturado da sessao 06:07, `gg_bypass2_s2c.log`)

### Envelope C3/C4 + SimpleModulus (S→C)

```
C3 <size:1>            [SM cifrado ...]        header de 2 bytes
C4 <sizeHi:1><sizeLo:1> [SM cifrado ...]       header de 3 bytes
```

O corpo cifrado e o SimpleModulus do servidor (chave de **recepcicao** do servidor = chave
de envio do servidor). O bloco decifrado comeca com um **byte de serial** e depois o pacote
interno (C1/C2) — o mesmo formato que o `C3Decoder` ja usa no sentido C→S (lá funciona).

O SimpleModulus **valida a si mesmo**: cada bloco tem um checksum. Então o byp pode testar
as chaves candidatas de recepcao (`SimpleModulusCipher::InitializeReceiveKeys`, que varre
`Obj1+0x48..0x800` procurando o prefixo `0x00D546EC`) e **aceitar a que passar no
checksum** — sem adivinhacao. Se nenhuma passar, o pacote simplesmente nao decodifica
(o modulo continua funcionando com o learn-and-replay).

### `F3 10` — lista de itens (o inventario)

Layout do servidor (`CItemManager::GCItemListSend`):

```
C2 <sizeHi> <sizeLo> F3 10 <count> [ <slot:1> + ItemInfo(12) ] * count
```

O ID do item e montado como no servidor (`ItemManager::ItemByteConvert`):

```cpp
id  = info[0];                                  // bits 0..7
id |= (info[3] & 0x02) << 7;                    // bit 8
id |= ((info[5] >> 3) & 0x0F) << 9;             // bits 9..12  (grupo = id >> 9)
```

> O servidor usa `GET_ITEM(grupo, idx) = grupo*512 + idx` (`MAX_ITEM_TYPE=512`), por isso os
> bits do **grupo** ficam em 9..12 — e nao em 8..11. (O `AutoPotion::DecodeItemId` ja faz
> isso; falta so o `F3 10` chegar.)

### Tabela de potions (servidor eMU `ObjectManager::CharacterUsePotion`)

| ID (`GET_ITEM(14,x)`) | Tipo | Restauracao |
|---|---|---|
| `14,0` | HP | `ApplePotionRate` % do MaxLife |
| `14,1 / 14,2 / 14,3` | HP | Small / Middle / Large Life % |
| `14,4 / 14,5 / 14,6` | MP | Small / Middle / Large Mana % |
| `14,35 / 14,36 / 14,37` | SD | Small / Middle / Large Shield % |
| `14,38 / 14,39 / 14,40` | HP+SD | composta |
| `14,70` / `14,71` | HP / MP | elite |
| `14,133` | SD | elite |

O ID (`grupo << 9 | idx`) de um item `14,x` e `14*512 + x = 7168 + x`.

## Algoritmo

### 1. Descobrir o slot (sem depender do usuario)

```
a cada pacote F3 10 recebido:
    para cada (slot, id):  g_slotItemId[slot] = id
    reclassifica:  g_hpSlots = { slots cujo id ∈ {7168..7171} ∪ {7238} }
                   g_mpSlots = { slots cujo id ∈ {7172..7174} ∪ {7239} }
    escolhe o slot com a maior taxa de restauracao conhecida
        (Small > Middle > Large nao e "maior" — e o que o jogador tem;
         sem a taxa do servidor, mantem o primeiro slot valido encontrado)
```

O learn-and-replay continua como **fallback**: se nao houver inventario, o modelo aprendido
(pelo `C1 06 26` do cliente + o pico de life/mana) manda.

### 2. Trocar de slot quando acaba

O `F3 10` so chega no login. Para saber que o item **acabou**, o byp usa o sinal do
**servidor**: quando uma potion e usada, o servidor responde com o life/mana novo e
**consome o item** (`DecreaseItemDur`). O `F3 10` nao volta.

Sinalizacao disponivel no trafego:
- o **item deletado** no inventario — o servidor manda `GCItemDeleteSend` (o opcode do
  inventario/uso de item) quando o item e consumido;
- o **uso sem efeito**: se o life/mana nao subiu depois do uso, o servidor recusou
  (item vazio / anti-hack / slot invalido).

Politica proposta (em ordem):

1. **uso sem efeito** (o life/mana nao subiu no prazo) → marcar o slot como suspecto e
   **tentar o proximo slot** do mesmo tipo (round-robin), sem esperar o inventario;
2. **`F3 10` novo** → reclassificar tudo e escolher o melhor slot;
3. **sem mais slots do tipo** → logar `sem potion de HP no inventario` e parar de tentar
   (nao continuar injetando no slot vazio — hoje e o que acontece e so confunde).

### 3. Life/mana maximo

Fontes, em ordem de preferencia:

1. `F3 E1` (calc) → `ViewMaxHP`/`ViewMaxMP` (DWORD LE no offset 4 e 12 do payload);
2. `C1 26`/`C1 27` com `type = 0xFE` → o max (esse servidor nao manda, mas o cliente aceita);
3. **manter o ultimo max conhecido** (nao zerar quando um pacote novo nao traz max) — o
   max muda so em level up, e o `F3 E1` volta nessa hora.

> Correcao aplicada no design anterior: o `F3 E1` e um snapshot (pode ter 80s); o `C1 26`
> type `0xFF` e o valor atual. A decisao tem que usar **o mais recente de cada fonte**, nao
> um unico "fonte de verdade" — e `type 0xFE` nunca pode ser lido como vida atual.

## Impacto no modulo atual

- `TryDecodeSmBody`: passar a tratar `C4` (header 3 bytes) e a validar a chave pelo checksum;
- `ParseItemList`: ja existe — passa a ser alimentado de verdade;
- `DecodeItemId`: ja existe — so falta o `F3 10`;
- `BuildItemUsePacket`: hoje so usa `hpSlot`/`mpSlot`/`hpItemId`/`mpItemId`/modelo aprendido.
  Passa a ter um **slotsAutomaticos** (derivados do inventario) com prioridade sobre o
  learn-and-replay (que vira fallback).
