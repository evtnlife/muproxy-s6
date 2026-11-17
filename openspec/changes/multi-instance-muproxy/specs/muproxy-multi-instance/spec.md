# Spec Delta

## ADDED Requirements

### Requirement: Instâncias isoladas de MuProxy2
O sistema DEVE permitir que mais de uma instância do MuProxy2 seja executada ao mesmo
tempo, cada uma responsável por um processo de jogo (`main.exe`) distinto, sem que as
instâncias interfiram entre si.

#### Scenario: Duas instâncias com jogos diferentes
- **WHEN** duas instâncias do MuProxy2 estiverem abertas, cada uma com o seu `main.exe`
- **THEN** cada instância mostra somente os dados do seu próprio jogo (monstros, life/mana,
  estado do auto-potion)
- **AND** os dados de uma instância NÃO aparecem na outra

#### Scenario: Jogo lançado pela própria instância
- **WHEN** o usuário lança o jogo pelo botão "Launch Game" de uma instância
- **THEN** a instância aloca um par de portas livres, registra a instância do jogo
  (PID + portas) e o jogo passa a se comunicar com **aquela** instância

### Requirement: Portas resolvidas em runtime
O canal de comando (44412) e o canal de pacotes (44413) DEVE ter a porta resolvida em
runtime, e não fixa em tempo de compilação, para que várias instâncias possam coexistir.

#### Scenario: Instância sem registro de jogo
- **WHEN** uma instância do byp inicializar sem encontrar um registro de instância para o
  seu próprio PID
- **THEN** ele usa as portas padrão (44412/44413) e funciona como antes

#### Scenario: Registro de instância encontrado
- **WHEN** o byp encontrar um registro de instância para o seu PID
- **THEN** ele conecta nos canais de comando e de pacotes nas portas indicadas no registro

#### Scenario: Jogo já em execução é adotado
- **WHEN** o usuário adotar um `main.exe` que já estava rodando
- **THEN** a instância escreve o registro desse PID e passa a escutar as portas alocadas
- **AND** o byp do jogo, ao tentar conectar e falhar, re-resolve a porta e conecta sozinho,
  sem precisar reiniciar o jogo

### Requirement: Diagnóstico da resolução de porta
O byp DEVE registrar no log qual registro de instância leu (ou que não encontrou) e em
quais portas conectou.

#### Scenario: Registro lido
- **WHEN** o byp conectar com sucesso
- **THEN** o log contém o caminho do registro lido e as portas de comando e de pacotes
