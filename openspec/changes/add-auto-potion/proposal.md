# Proposal

## Why

O auto-attack do byp.dll já roda sozinho (0x11 e 0x19), mas o jogador precisa **beber
potion na mão**: se o monstro martela o life, o bot continua atacando até morrer. O cliente
BlackRock (ex603) não tem auto-potion confiável (o `MUHelper` do Webzen — struct com
`RecoveryPotionOn`/`RecoveryPotionPercent` — é só a interface do plugin, não vem
implementado), e o MuProxy2 só controla ataque/monstros.

O byp.dll já tem tudo que falta:

- lê **life/mana** do jogador local em memória (`GameMemory::Player::Life()/Mana()`);
- tem **uma porta única de injeção** (`PacketInjector::Inject`) que já sabe aplicar
  Camada-2 (XorData) + Layer-1 corretamente;
- tem o **fluxo de pacotes** capturados (SEND/RECV decifrados) via `PacketSniffer`.

O que falta é o pacote de uso de item e o gatilho. E o protocolo do consumível está
documentado no servidor de referência (eMU `GameServer`):

```
C1 06 26 <SourceSlot> <TargetSlot> <type>      // PMSG_ITEM_USE_RECV (opcode 0x26)
```

- `CItemManager::CGItemUseRecv` → `CObjectManager::CharacterUsePotion`:
  potions = itens `GET_ITEM(14,0..6)` (Apple/Small/Mid/Large Life, Small/Mid/Large Mana),
  `14,35..40` (SD e compostos), `14,70/71/133` (elite). O servidor **aplica o efeito
  direto** (HP%/MP%/SD% do `MaxLife`/`MaxMana`) e manda o life/mana update;
- **anti-hack**: uma segunda potion dentro de `CheckAutoPotionHackTolerance` ms é
  **rejeitada** (e o item **não** é consumido) → o auto-potion precisa respeitar um intervalo;
- o servidor **não** decide quando beber: o limiar é 100% client-side.

## What Changes

- **Novo módulo `AutoPotion`** (`src/potion/AutoPotion.cpp` + `src/include/AutoPotion.h`),
  namespace própria, na pasta de domínio `potion/`:
  - thread watcher (tick 100 ms) lê HP/MP da memória do cliente e dispara o uso;
  - limiares configuráveis (HP%, MP%) e intervalo mínimo entre usos (anti-hack);
  - comandos `POTION_START`, `POTION_STOP`, `POTION_CONFIG`, `POTION_STATUS`.
- **Opcode 0x26 na porta única de injeção**: `PacketInjector::Inject` passa a aceitar o
  uso de item (Camada-2 [3,len) + Layer-1, sem SimpleModulus) — nenhuma porta nova.
- **`MuCipher::UnXorData`**: o inverso da Camada-2 (mesma conta, ordem decrescente), para
  **decodificar** pacotes capturados (o `XorData` do jogo só codifica; o servidor decodifica
  na ordem inversa).
- **Aprendizado do slot ("learn & replay")**: o inventário do cliente não está mapeado
  (não faz parte do `ObjectCharacter`), então o módulo **observa** o `C1 06 26` que o
  próprio cliente manda quando o jogador bebe a potion, decodifica o payload e guarda o
  pacote como modelo. Comparando o life/mana antes e depois, ele descobre se aquele slot é
  de **HP** ou de **MP**. O jogador bebe cada potion uma vez e pronto. O slot também pode
  ser configurado à mão.
- **MuProxy2 (C#)**: classe de mensagem + roteamento + um grupo de UI (ligar/desligar,
  limiar de HP, limiar de MP, intervalo, botão de status).

## Capabilities

### New Capabilities
- `auto-potion`: beber potion automaticamente quando HP/MP ficam abaixo do limiar,
  respeitando o intervalo mínimo entre usos.

### Modified Capabilities
(nenhuma — o ataque, os pacotes existentes e o protocolo 44412 não mudam)

## Impact

- **Código afetado**:
  - novo `src/potion/AutoPotion.cpp` + `src/include/AutoPotion.h`;
  - `src/packets/PacketInjector.cpp` (opcode 0x26 no switch);
  - `src/crypto/MuCipher.{h,cpp}` (`UnXorData`);
  - `src/packets/PacketSniffer.cpp` (notifica o uso de item observado);
  - `src/core/dllmain.cpp` + `build.bat` (registro do módulo);
  - `byp/MuProxy2/**` (mensagem, roteamento, UI).
- **Protocolo**: novo opcode na porta de injeção, mas **nenhum byte no wire muda** para os
  opcodes já existentes (0x11/0x19) — o `switch` só ganha um `case`.
- **Dependências**: nenhuma nova.
- **Risco**: médio-baixo. O maior risco é **errar o slot** (o servidor ignora o item se não
  for potion — nada é consumido, sem punished) e **tomar anti-hack** (intervalo curto) — o
  padrão de 1000 ms é conservador. O campo ainda precisa de validação (beber a potion e
  conferir o life/mana subindo).
- **Fora de escopo**: ler o inventário do cliente (RE nova), poções de elite/SD/compostas
  (o protocolo é o mesmo, mas o aprendizado automático foi feito só para HP/MP), detecção
  de "sem potion no inventário" (o servidor simplesmente não faz nada).
