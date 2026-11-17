# Spec Delta

## ADDED Requirements

### Requirement: Exibição do HWID enviado no login
O sistema DEVE identificar o HWID que o cliente envia no login e exibi-lo na interface, com
o pacote e o offset em que ele foi encontrado.

#### Scenario: HWID detectado no login
- **WHEN** o cliente enviar o pacote de login e o payload contiver o padrão de HWID
  (`8HEX-8HEX-8HEX-8HEX`)
- **THEN** o byp envia `HWID|value=..|offset=..|opcode=..` para a UI
- **AND** a UI mostra o valor detectado como somente-leitura

#### Scenario: HWID não reconhecido
- **WHEN** o payload do login não contiver o padrão de HWID
- **THEN** o byp registra no log que o HWID não foi reconhecido
- **AND** o byp NÃO altera o pacote de login

### Requirement: Substituição do HWID
O usuário DEVE poder substituir o HWID enviado no login por um valor escolhido por ele, e
o sistema DEVE aplicar essa substituição no pacote de login antes de enviá-lo ao servidor.

#### Scenario: Valor customizado aplicado
- **WHEN** houver um HWID customizado válido configurado e o cliente enviar o pacote de login
- **THEN** o byp substitui os bytes do HWID no payload, re-cifra a Layer-1 e envia o pacote
  ao servidor

#### Scenario: Valor customizado inválido
- **WHEN** o valor configurado não tiver o formato `8HEX-8HEX-8HEX-8HEX`
- **THEN** a UI recusa o valor
- **AND** o byp continua enviando o HWID original do cliente

#### Scenario: Removendo o valor customizado
- **WHEN** o usuário limpar o HWID customizado
- **THEN** o byp volta a enviar o HWID original do cliente, sem alterar o pacote

### Requirement: Isolamento por instância
O HWID customizado DEVE ser aplicado apenas à instância de jogo à qual pertence, sem
afetar outras instâncias do MuProxy2.

#### Scenario: Duas instâncias com HWIDs diferentes
- **WHEN** duas instâncias estiverem configuradas com HWIDs customizados distintos
- **THEN** cada uma aplica o seu valor no próprio jogo
