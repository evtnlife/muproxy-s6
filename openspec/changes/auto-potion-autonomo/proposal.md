# Proposal

## Why

O auto-potion funciona, mas depende de um passo manual que o usuario acha inexplicavel:
**beber uma potion de cada tipo uma vez** para o byp "aprender" o slot ("learn & replay").
Sem isso o modulo fica em `sem modelo de potion aprendido` e nunca dispara. E o pior:
quando a potion do slot acaba, o byp **continua injetando no slot vazio** — o servidor ignora
(o item nao e potion → nao consome, nao aplica efeito), o log ainda diz "disparada" e o
jogador nao ve nada acontecer.

A causa raiz das duas coisas e a mesma: **o byp nunca recebe o inventario** (`F3 10`).

Captura do trafego bruto (`gg_bypass2_s2c.log`, sessao 06:07):

```
RECV #9   op=F3 sub=00 len=248  C2 00 F8 F3 00 ...   <- lista de personagens (clara)
RECV #12  op=F3 sub=E1 len=244  C1 F4 F3 E1 D6 00 00 00 D6 ...  <- calc: maxLife=214, maxMana=219 (clara)
RECV #15  op=7F len=233  C3 E9 7F B1 ...             <- pacote GRANDE (C3+SimpleModulus)
RECV #16  op=48 len=256  C4 03 10 48 FE ...         <- pacote GRANDE (C4+SimpleModulus)
```

O `F3 10` (lista de itens) **nao aparece em claro em nenhum ponto** — ele vem dentro de um
dos pacotes grandes (C3/C4 + SimpleModulus). E o byp **nao decifra C3/C4 no sentido S→C**:

- `AutoPotion::TryDecodeSmBody` so trata `C3` — o `C4` tem header de **3 bytes** (C4, sizeHi,
  sizeLo) e nunca chega perto de ser tentado;
- a decifra S→C (`SimpleModulusCipher::DecryptReceive`) nunca foi validada contra um pacote
  real: a sessao inteira tem **1 C3 e 1 C4** no trafego S→C.

Consequencia: sem `F3 10` o byp nao sabe o ID do item de cada slot → nao sabe qual slot tem
potion de HP/MP → so pode descobrir pela via "o jogador bebeu e o life/mana subiu".

## What Changes

- **Decifrar C3/C4 no S→C** (`AutoPotion` + `SimpleModulusCipher`): tratar C3 (header 2) e
  C4 (header 3), testar as chaves candidatas de recepcao e aceitar a que passar no **checksum
  do SimpleModulus** (o proprio protocolo valida a chave — nao precisa adivinhar).
- **Parsear o `F3 10`** (lista de itens) → tabela `slot → item ID`, decodificando o ID com a
  mesma conta do servidor (`ItemManager::ItemByteConvert`).
- **Classificar a potion pelo ID** (tabela do `CharacterUsePotion`): `14,0..3` = HP,
  `14,4..6` = MP, `14,38..40` = composta (HP+SD), `14,70/71` = elite. Escolher
  automaticamente o slot com a maior taxa de restauracao → **funciona sem beber nada**.
- **Troca automatica quando a potion acaba**: monitorar o inventario; quando o ID do slot
  mudar (acabou/foi movido/trocado) escolher outro slot do mesmo tipo e registrar no log.
- **Deteccao de uso sem efeito**: se o uso injetado nao produzir vida/mana, revalidar o slot
  (o servidor pode ter recusado) e escolher outro.
- **Manter o learn-and-replay como fallback** quando o inventario nao estiver disponivel.

## Capabilities

### New Capabilities
(nenhuma — e uma evolucao do `auto-potion`)

### Modified Capabilities
- `auto-potion`:
  - a descoberta do slot passa a ser **automatica** (pelo ID do item no inventario), com o
    learn-and-replay como fallback;
  - passa a **trocar de slot** quando a potion acaba;
  - passa a **reagir a uso sem efeito**.

## Impact

- **Codigo afetado**:
  - `src/potion/AutoPotion.cpp` (decifra C3/C4, parse do F3 10, escolha/troca de slot);
  - `src/crypto/SimpleModulusCipher.{h,cpp}` (decifra S→C com validacao por checksum);
  - `src/packets/PacketSniffer.cpp` (o F3 10 precisa chegar ao parser);
  - `byp/MuProxy2/**` (mostrar o slot escolhido e o motivo da troca).
- **Protocolo**: nada muda no wire — e leitura de trafego que passa a existir.
- **Risco**: **medio**. O ponto critico e a decifra do S→C; o checksum do SimpleModulus
  protege contra chave errada (o pacote simplesmente nao decodifica e o byp segue com o
  fallback). O risco de errar o slot e baixo: com o ID do item a classificacao e exata.
- **Fora de escopo**: SD/compostas/elite como alvo do auto-potion (o protocolo e o mesmo, mas
  o limiar continua sendo HP%/MP%); leitor de inventario do cliente em memoria (fallback se o
  F3 10 nao for decodificavel).
