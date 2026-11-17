# Tasks

> Cada fase = build + teste em campo. A verificação está embutida em cada task.
> A Fase 1 é uma **captura**: nada de código até saber qual é o pacote de login.

## 1. Fase 1 — Captura: onde está o HWID

- [ ] 1.1 Reproduzir o login com o `MuProxy2` aberto e salvar as linhas `PKT|SEND|hex` do
  pacote de login (aba "Packets" ou `gg_bypass2_s2c.log`).
- [ ] 1.2 Procurar o padrão `8HEX-8HEX-8HEX-8HEX` no hex (em claro e após a Layer-1) e
  anotar: **opcode**, **offset** e o resto do payload (usuário/senha/tamanho).
- [ ] 1.3 Confirmar se o mesmo padrão aparece em **outros** pacotes (select character /
  connect no game server) — a resposta muda o escopo da reescrita.

## 2. Fase 2 — Detectar e mostrar

- [ ] 2.1 byp: novo módulo `src/packets/HwidManager.{h,cpp}`:
  - `FindInPayload(const uint8_t* plain, int len, int* offset)` → procura o padrão
    `8HEX-8HEX-8HEX-8HEX` (35 bytes ASCII);
  - `DetectAndReport(...)` → chamado no caminho de envio (C→S), para o pacote de login;
    loga o valor, o offset e o opcode.
- [ ] 2.2 byp: enviar `HWID|value=..|offset=..|opcode=..|active=..` para a UI pelo
  `CommandChannel` (44412) — tag nova, registrada no `MessageRegistry`.
- [ ] 2.3 `MuProxy2/Protocol/Messages.cs`: classe `HwidMessage` (tag `HWID`) + parse;
  `DllCommandService` → evento `OnHwid`.
- [ ] 2.4 `MainForm.cs` + `MainForm.Designer.cs`: painel "HWID" na aba "Custom Stuff" com o
  **valor detectado** (read-only), o **valor em uso** e o status (detectado/customizado).
- [ ] 2.5 Teste de campo: ao logar, a UI mostra o HWID que o cliente mandou.

## 3. Fase 3 — Alterar o HWID

- [ ] 3.1 byp: `HWID_SET|value=...` grava o valor customizado (validando o formato
  `8HEX-8HEX-8HEX-8HEX`); `HWID_CLEAR` remove. O valor fica no **registro de instância**
  (`instances\<pid>.txt`, chave `hwid=`) — ou em memória, se o change de multi-instância
  ainda não existir.
- [ ] 3.2 byp: no caminho de envio, se o pacote for de login e houver valor customizado:
  decifrar a Layer-1 → localizar o padrão → **substituir os 35 bytes** → re-cifrar a Layer-1
  → seguir o caminho normal. Se o padrão não for encontrado → **não mexe** e loga.
- [ ] 3.3 `MuProxy2/MainForm.cs`: campo editável com validação + botão "usar" + botão
  "gerar" (4 DWORDs aleatórios no formato) + botão "voltar ao detectado".
- [ ] 3.4 Teste de campo: (a) com o valor detectado, o login funciona como antes;
  (b) com um valor customizado, o login **passa** e o servidor grava o novo valor
  (conferir no log do servidor/DB ou no comportamento do cliente);
  (c) com um valor customizado inválido, a UI recusa e o byp não altera nada.

## 4. Fase 4 — Fechamento

- [ ] 4.1 Conferir que o tráfego **não-login** sai byte a byte igual (comparar o
  `gg_bypass2_s2c.log` antes/depois, exceto o pacote de login).
- [ ] 4.2 Documentar no `skill` (referência 13) o layout do pacote de login e o offset do
  HWID (achado na Fase 1) — para não redescobrir depois.
