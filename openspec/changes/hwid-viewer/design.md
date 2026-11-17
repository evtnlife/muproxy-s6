# Design

## Context

### O que é o HWID (e de onde vem)

Algoritmo clássico Webzen/eMU — o mesmo do `GetHardwareId/GetHardwareId.cpp` e do
`AuthServer/Util.cpp` no `MuEmu-master`:

```
HWID1 = GetVolumeInformation("C:\")            ^ 0x12B586FE
HWID2 = uuid.Data4[2]                          ^ 0x5D78A569
HWID3 = uuid.Data4[6] | (arch   << 16)         ^ 0xF45BC123
HWID4 = (procLevel | (revision << 16))         ^ 0xB542D8E1
→  wsprintf("%08X-%08X-%08X-%08X")   // 35 chars
```

Onde isso vive no cliente:

| peça | achado | escopo |
|---|---|---|
| `Main.dll` | tem a string `"%08X-%08X-%08X-%08X"`, `"BIN"` e `"Software\Webzen\Mu\Config"` | o cliente **cacheia** o HWID no registro |
| `BlackrockMU.exe` (launcher .NET) | métodos `GetHWID` e `SendHWIDToServer` | o launcher manda o HWID para o backend **dele** (anti-cheat próprio) |
| `HKCU\Software\Webzen\Mu\Config` | existe, **sem** o valor `BIN` nesta máquina | o cache não está presente agora |

⚠️ `Main.dll` tem CRC/checksum próprio (`CheckSum.premium`, `ScriptSystem.premium file CRC
error`): **não dá para patchear o arquivo do cliente**.

### O que o byp já tem

- hook **global** em `send`/`WSASend`/`sendto` (`src/net/winsock/WinsockHooks.cpp:85`): todo
  pacote do cliente passa pelo byp, inclusive o de login (que vai para o **login server**,
  antes do game server);
- cada pacote vira uma linha `PKT|SEND|hex` na UI (aba "Packets");
- `MuCipher::DecryptLayer1` / `EncryptLayer1` para desfazer/refazer a Layer-1;
- o `CommandChannel` (44412) para mandar mensagens byp→UI.

## Goals / Non-Goals

**Goals:**
- mostrar o HWID **real** que o cliente manda no login;
- permitir trocar por um valor **escolhido** pelo usuário, ou gerar um aleatório;
- não alterar nenhum arquivo do cliente;
- não quebrar o login: se algo não for reconhecido, o byp **não mexe** no pacote.

**Non-Goals:**
- mexer no HWID que o launcher manda para o backend anti-cheat;
- alterar o HWID já gravado no banco do servidor (fora do alcance do cliente);
- editar o pacote de login à mão (já existe `SEND_PACKET` para isso).

## Decisions

### D1 — Alterar no **wire**, não na origem

Reescrever o campo no pacote de login, em vez de:
- mexer no registro (`BIN`) → depende de o cliente reler o cache (e do formato do blob);
- patchear `Main.dll` → quebra o CRC/checksum do cliente;
- patchear o launcher → é um app .NET, além de ser o canal de anti-cheat.

O wire é o único ponto onde o byp tem **controle total** e **reversibilidade** (troca só o
que o servidor lê).

### D2 — Detectar o HWID por **padrão**, não por offset fixo

O payload do login é procurado por `8HEX-8HEX-8HEX-8HEX` (ASCII, 35 bytes). Vantagens:
- não depende do opcode/offset exatos do pacote de login (que ainda não foram medidos);
- sobrevive a mudança de versão do cliente;
- o único falso-positivo possível é uma string parecida em outro campo — por isso a UI mostra
  **o offset** e o pacote, e o usuário confirma.

Se o padrão não for encontrado → o byp **não reescreve nada** (fail-safe).

### D3 — O valor customizado vive no **registro de instância**

O HWID customizado é gravado no mesmo arquivo `instances\<pid>.txt` do
change `multi-instance-muproxy` (chave `hwid=`). Consequência: **cada instância tem o seu
HWID** → duas contas, dois HWIDs, sem estado global.

Sem o change de multi-instância: a UI mantém o valor em memória e o manda por
`HWID_SET` a cada conexão do byp.

### D4 — A reescrita acontece **no envio**, antes do socket

O hook de `send` recebe o buffer **cifrado** (Layer-1). O caminho é:
1. decifrar a Layer-1 → payload em claro;
2. se for o pacote de login e houver HWID customizado:
   - localizar o padrão (mesma rotina da detecção) e substituir os 35 bytes;
   - re-cifrar a Layer-1;
3. mandar para o socket (o buffer original, já modificado, segue o caminho normal).

O byp **não** mexe em pacotes que não sejam de login, e o `POTION_STATE`/`PKT` seguem
intactos.

### D5 — A UI mostra os **dois** valores

- **detectado**: o que o cliente mandou de verdade (o original, sempre visível);
- **em uso**: o que vai ser enviado (o detectado, ou o customizado).

Assim o usuário nunca perde a referência do valor real (pode querer voltar a ele).

### D6 — Validação estrita do formato

O campo aceita apenas `XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX` (8 hex + 3 hífens, case
insensitive, 35 chars). O botão "gerar" produz 4 DWORDs aleatórios nesse formato.

## Risks / Trade-offs

| risco | mitigação |
|---|---|
| o payload do login não é decifrável com a Layer-1 (o pacote pode ter outra cifra) | o byp detecta e avisa no log ("HWID: payload não reconhecido") e **não altera nada** |
| o HWID também pode estar em outro pacote (ex.: connect no game server) | a rotina é genérica: procura o padrão em **todo** pacote C→S; a UI mostra em qual pacote/opcode ele apareceu |
| reescrever pode invalidar algum checksum do login | o MU clássico não tem checksum no login; se o servidor recusar, o log mostra o erro e o usuário volta ao valor detectado |
| o HWID é usado em mais de um ponto do fluxo | o log lista **todos** os pacotes onde o padrão apareceu (com opcode/offset) — a UI mostra a lista |
| uso indevido (contas múltiplas / burlar ban) | escopo do usuário: a conta continua sendo do servidor; isto só muda o identificador de máquina que o cliente se declara ter |

## open questions

- **qual é o pacote de login** (opcode + layout)? → sai de **uma captura**: loga no
  `PKT|SEND|hex` e procura o padrão (a UI já mostra o hex).
- o HWID é enviado **só** no login ou também no "select character"/"connect"? → a rotina de
  detecção responde isso sozinha (ela reporta todo pacote onde o padrão aparecer).
- o valor precisa ter o mesmo **case** do original (`%08X` = maiúsculo)? → o servidor compara
  string; o byp manda sempre em maiúsculo.
