# Spec Delta

## ADDED Requirements

### Requirement: Tema e identidade visual
A interface DEVE ter um tema escuro próprio, aplicado de forma consistente a todos os
controles, com tipografia legível e uma cor de destaque.

#### Scenario: Aplicação do tema
- **WHEN** o MuProxy2 é aberto
- **THEN** todos os painéis, campos, botões, listas e abas usam a paleta do tema
- **AND** o texto sobre o fundo escuro mantém contraste legível

#### Scenario: Tema aplicado sem alterar o comportamento
- **WHEN** o tema é aplicado
- **THEN** nenhuma funcionalidade muda de comportamento (apenas cor, fonte e forma)

### Requirement: Inputs organizados
Os campos de entrada DEVEM ter o rótulo acima do campo, alinhados em grade e agrupados
por cards, para que a leitura da interface seja imediata.

#### Scenario: Leitura de um formulário
- **WHEN** o usuário olha um grupo de campos (ex.: limiares do auto-potion)
- **THEN** cada campo tem o rótulo acima, com larguras iguais e alinhamento consistente

### Requirement: Feedback de estado visível
A interface DEVE indicar visualmente o estado das conexões e do auto-potion, sem exigir
que o usuário leia o log.

#### Scenario: byp desconectada
- **WHEN** o byp.dll desconectar (jogo fechado)
- **THEN** o indicador de `BYP` fica vermelho

#### Scenario: byp conectada
- **WHEN** o byp.dll conectar
- **THEN** o indicador de `BYP` fica verde

#### Scenario: auto-potion ligado
- **WHEN** o auto-potion estiver ativo (mensagem `POTION_STATE` com `active=1`)
- **THEN** o indicador de `POTION` fica verde

### Requirement: Header do personagem
A aba principal DEVE exibir em destaque a identidade do personagem (nome, level, mapa e
posição), refletindo o que a interface mostra no título da janela.

#### Scenario: Dados do jogador disponíveis
- **WHEN** nome, level, mapa e posição estiverem disponíveis
- **THEN** o header mostra os quatro valores de forma legível

#### Scenario: Dados ainda indisponíveis
- **WHEN** o byp ainda não tiver enviado os dados do jogador
- **THEN** o header mostra um estado de espera, sem travar a interface
