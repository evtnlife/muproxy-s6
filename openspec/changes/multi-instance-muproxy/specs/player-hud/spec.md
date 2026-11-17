# Spec Delta

## ADDED Requirements

### Requirement: Identidade do jogador local
O byp DEVE expor a identidade do jogador local (nome, level e mapa) para a UI por meio de
uma mensagem dedicada, para que a interface possa identificar a qual personagem ela está
ligada.

#### Scenario: Personagem identificado
- **WHEN** o byp receber as informações do personagem (nome pelo pacote de nome do
  personagem, level pelo character info `F3 E0`, mapa pelo pacote de troca de mapa)
- **THEN** o byp envia `PLAYER|name=..|level=..|map=..` para a UI

#### Scenario: Atualização do estado do jogador
- **WHEN** algum dado do jogador mudar (level, mapa, nome)
- **THEN** o byp reenvia a mensagem `PLAYER` com os valores atualizados

### Requirement: Título da janela com o personagem
A janela do MuProxy2 DEVE mostrar no título a identificação do personagem que ela
controla, no formato `MuProxy | <Nome> | <Level> | <Mapa> | <X>, <Y>`.

#### Scenario: Personagem conhecido
- **WHEN** a instância tiver nome, level, mapa e posição do jogador local
- **THEN** o título da janela mostra `MuProxy | <Nome> | <Level> | <Mapa> | <X>, <Y>`

#### Scenario: Personagem ainda desconhecido
- **WHEN** a instância ainda não tiver os dados do jogador
- **THEN** o título mostra `MuProxy | (sem char)` e é atualizado assim que os dados chegam

### Requirement: Header do personagem na interface
A interface DEVE exibir os mesmos dados do jogador (nome, level, mapa e posição) em um
painel de destaque, para que o usuário identifique a instância sem olhar o título.

#### Scenario: Atualização do header
- **WHEN** a posição do jogador local mudar
- **THEN** o header atualiza X e Y sem alterar nome, level e mapa
