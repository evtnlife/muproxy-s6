# Proposal

## Why

O cliente BlackRockMU manda um **HWID** no login — o identificador clássico
Webzen/eMU, calculado com o serial do volume de `C:\`, um UUID e os dados do processador,
formatado como `%08X-%08X-%08X-%08X` (35 caracteres). O servidor usa esse valor para
**vincular a conta a uma máquina**.

Problemas práticos do usuário:
- ele **não sabe** qual HWID o seu cliente está mandando (não tem como ver, nem log);
- se o Windows foi reinstalado (ou o volume/UUID mudou), o HWID muda e a conta pode ser
  tratada como " HWID diferente" — o usuário precisa **ver** o valor para restaurar o
  anterior;
- se quiser rodar duas contas com o mesmo cliente, precisa de **dois** HWIDs diferentes.

O `MuProxy2` já tem a peça que falta: o byp hooka `send`/`WSASend`/`sendto` **globalmente**
(`src/net/winsock/WinsockHooks.cpp:85`), então **todo** pacote do cliente — inclusive o de
login — passa por ele e já aparece na UI como `PKT|SEND|hex`. E o byp tem
`MuCipher::DecryptLayer1` para desfazer a cifragem do payload.

## What Changes

- **Ver o HWID**: o byp procura, no pacote de login decifrado, o padrão do HWID
  (`8HEX-8HEX-8HEX-8HEX`) e manda para a UI (`HWID|value=..|offset=..|packet=..`).
- **Alterar o HWID**: a UI tem um campo editável (com validação do formato) + um botão
  "gerar" (HWID aleatório válido). O valor customizado é gravado no **registro de
  instância** do jogo (por instância → duas contas, dois HWIDs).
- **Aplicar no login**: se houver HWID customizado, o byp **reescreve o campo** no pacote
  de login antes de mandá-lo ao servidor (re-cifrando a Layer-1). Nada mais do tráfego é
  tocado.
- **Comandos**: `HWID_SET|value=...` (define o customizado) e `HWID_CLEAR` (volta ao valor
  real do cliente).

## Capabilities

### New Capabilities
- `hwid-management`: exibir o HWID que o cliente envia no login e poder substituí-lo por
  um valor escolhido pelo usuário.

### Modified Capabilities
(nenhuma)

## Impact

- **Código afetado**:
  - `byp/gg_bypass2/src/packets/` (novo módulo `HwidManager` — ou em `src/game/`, conforme
    o domínio: é tráfego de login, não memória do jogo);
  - `byp/gg_bypass2/src/crypto/MuCipher.cpp` (decifrar/re-cifrar a Layer-1 do pacote de
    login — as funções já existem);
  - `byp/MuProxy2/MuProxy2/Protocol/Messages.cs` + `Services/` (tag `HWID`);
  - `byp/MuProxy2/MuProxy2/MainForm.cs` + `MainForm.Designer.cs` (painel do HWID).
- **Protocolo**: nova tag byp→UI `HWID`; o pacote de login do **cliente→servidor** é
  reescrito (é o único dado alterado em todo o tráfego).
- **Cliente**: **nenhum arquivo do cliente é alterado** (nem `main.exe`, nem `Main.dll`, nem
  o launcher) — o CRC/checksum do `Main.dll` continua válido.
- **Risco**: **médio**. Depende de acertar (a) o pacote de login e (b) o offset do campo —
  ambos confirmados por **uma captura** antes de codificar. Se o padrão não for encontrado,
  o byp **não mexe em nada** (fail-safe: nunca quebrar o login por falta de informação).
- **Fora de escopo**:
  - o HWID que o **launcher** (`BlackrockMU.exe`, app .NET) manda para o backend dele
    (`SendHWIDToServer`) — é um canal separado e **não** é afetado por aqui;
  - o HWID do lado do **servidor** (a conta já gravada no banco continua com o HWID
    antigo; este change só muda o que o cliente manda a partir de agora);
  - burp/hex-edição manual do pacote de login pela UI (existe `SEND_PACKET` para isso).
