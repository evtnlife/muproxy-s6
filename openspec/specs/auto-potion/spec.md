# auto-potion Specification

## ADDED Requirements

### Requirement: Bebida automática de potion por limiar
O sistema DEVE consumir automaticamente uma potion quando o life/mana do jogador local
cair abaixo do limiar configurado, sem interação do usuário.

#### Scenario: Life abaixo do limiar
- **WHEN** o percentual de life do jogador local ficar abaixo do limiar de HP
  configurado e houver um modelo de potion de HP conhecido
- **THEN** o sistema injeta o pacote de uso de item (`C1 06 26`) correspondente
  no socket do game server
- **AND** o efeito da potion é aplicado pelo servidor (o life sobe)

#### Scenario: Mana abaixo do limiar
- **WHEN** o percentual de mana do jogador local ficar abaixo do limiar de MP
  configurado e houver um modelo de potion de MP conhecido
- **THEN** o sistema injeta o pacote de uso de item (`C1 06 26`) correspondente
- **AND** o efeito da potion é aplicado pelo servidor (o mana sobe)

#### Scenario: Jogador morto
- **WHEN** o life do jogador local for 0
- **THEN** o sistema NÃO injeta uso de item (o servidor rejeita uso com `DieRegen`)

### Requirement: Intervalo mínimo entre usos (anti-hack)
O sistema DEVE respeitar um intervalo mínimo entre dois usos de potion, para não ser
rejeitado pelo anti-hack de potion do servidor (`CheckAutoPotionHackTolerance`).

#### Scenario: Uso dentro do intervalo
- **WHEN** o life fica abaixo do limiar menos de `interval` ms após o último uso
- **THEN** o sistema NÃO injeta outro pacote de uso de item

#### Scenario: Uso fora do intervalo
- **WHEN** o life fica abaixo do limiar mais de `interval` ms após o último uso
- **THEN** o sistema injeta o pacote de uso de item

### Requirement: Descoberta do slot do inventário
O sistema DEVE descobrir o slot do inventário da potion a partir do próprio tráfego do
jogo (o pacote `C1 06 26` que o jogador envia ao beber a potion), decodificando o
payload (Camada-2 inversa) e classificando o modelo como de HP ou de MP pela variação
de life/mana.

#### Scenario: Jogador bebe uma potion de HP
- **WHEN** o cliente envia `C1 06 26` e o life do jogador local aumenta
- **THEN** o sistema guarda aquele pacote como o modelo de HP

#### Scenario: Jogador bebe uma potion de MP
- **WHEN** o cliente envia `C1 06 26` e o mana do jogador local aumenta
- **THEN** o sistema guarda aquele pacote como o modelo de MP

#### Scenario: Slot configurado manualmente
- **WHEN** o slot da potion é configurado por comando (`hpslot`/`mpslot`)
- **THEN** o sistema usa esse slot em vez do modelo aprendido

### Requirement: Comandos de controle
O sistema DEVE expor comandos tipados no canal 44412 para ligar, desligar, configurar e
consultar o estado do auto-potion.

#### Scenario: Ligar
- **WHEN** o comando `POTION_START` é recebido
- **THEN** o auto-potion passa a monitorar life/mana e a injetar usos

#### Scenario: Desligar
- **WHEN** o comando `POTION_STOP` é recebido
- **THEN** o auto-potion para de injetar usos

#### Scenario: Configurar
- **WHEN** o comando `POTION_CONFIG` é recebido com limiares/intervalo/slots
- **THEN** o sistema passa a usar esses valores no próximo ciclo

#### Scenario: Consultar estado
- **WHEN** o comando `POTION_STATUS` é recebido
- **THEN** o sistema registra no log os limiares, o intervalo, os modelos conhecidos e
  o life/mana atual

### Requirement: Injeção pela porta única
O pacote de uso de item DEVE ser injetado pela mesma porta de injeção dos demais
opcodes, com a Camada-2 (XorData) aplicada exatamente uma vez.

#### Scenario: Pacote de uso de item injetado
- **WHEN** o auto-potion injeta um uso de item
- **THEN** o payload é cifrado com Camada-2 sobre `[3, len)` e depois com a Layer-1,
  antes de ser enviado no socket do game server
