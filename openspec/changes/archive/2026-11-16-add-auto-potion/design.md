# Design

## Context

O byp.dll (C++ x86, injetado no cliente MU ex603) já injeta pacotes de ataque pela porta
única `PacketInjector::Inject` (Camada-2 `XorData` uma vez + Layer-1, e SimpleModulus no
0x19) e lê life/mana do jogador local em memória (`GameMemory::Player`, offsets do
`UserObjectStruct`: `OFF_LIFE 0x22`, `OFF_MAXLIFE 0x26`, `OFF_MANA 0x24`,
`OFF_MAXMANA 0x28`).

O protocolo do consumível vem do servidor de referência (eMU `GameServer`):

```
PMSG_ITEM_USE_RECV (C1:26)
  PBMSG_HEAD header;   // C1, size=6, head=0x26
  BYTE SourceSlot;     // slot do item no inventário (flat do servidor)
  BYTE TargetSlot;     // alvo (0xFF = nenhum); SourceSlot == TargetSlot → rejeitado
  BYTE type;           // usado só por Fruit (0x13,0x0F): 0 = +ponto, ≠0 = -ponto
```

`CItemManager::CGItemUseRecv` → `CObjectManager::CharacterUsePotion`:

| Item | Efeito |
|------|--------|
| `GET_ITEM(14,0)` | HP% (`ApplePotionRate`) |
| `GET_ITEM(14,1..3)` | HP% (Small/Middle/Large Life) |
| `GET_ITEM(14,4..6)` | MP% (Small/Middle/Large Mana) |
| `GET_ITEM(14,35..37)` | SD% (Small/Middle/Large Shield) |
| `GET_ITEM(14,38..40)` | HP% + SD% (compostas) |
| `GET_ITEM(14,70/71/133)` | elite HP% / MP% / SD% |

Regras do servidor que o cliente precisa respeitar:

- `SourceSlot` tem que ser um slot válido **com potion** — se não for, o efeito não
  acontece e o item **não** é consumido (sem penalidade);
- `SourceSlot == TargetSlot` → o pacote é ignorado;
- **anti-hack**: `GetTickCount() - PotionTime < CheckAutoPotionHackTolerance` → o uso é
  **rejeitado** (e a potion **não** é gasta);
- o servidor **não** olha o HP atual: ele aplica o efeito sempre que o pacote chega. O
  limiar é decisão do cliente.

Sobre a Camada-2: o `SocketManager` do servidor passa **todo** pacote recebido por
`PacketManager::ExtractPacket`, que faz `XorData(size-1, end)` — ou seja, **descendente**,
cobrindo `[3, size)` para C1. O cliente aplica o mesmo `XorData` **ascendente** em
`[3, size)` (fonte `Main_EX603/Main/PacketManager.cpp`). Ou seja: **todo** pacote C1 do
cliente para o servidor passa por Camada-2 — inclusive o `C1 06 26`. (Isso também fecha a
questão aberta da skill sobre o pacote de movimento.)

## Goals / Non-Goals

**Goals:**
- Beber HP/MP automaticamente quando o life/mana cair abaixo do limiar.
- Não inventar bytes: o pacote injetado tem que ser **byte a byte** o que o cliente envia.
- Respeitar a porta única de injeção e a porta única de XOR (regra do projeto).
- Configurável em runtime pelo MuProxy2, sem recompilar.
- Módulo isolado, no padrão de pasta por domínio (`src/potion/`).

**Non-Goals:**
- Não mapear o inventário do cliente (RE nova, fora do escopo desta fase).
- Não tratar SD, poções de elite, compostos, antídoto ou cura de TP.
- Não mexer no `AttackModule` nem no protocolo do ataque 0x11/0x19.
- Não tocar no servidor (o byp é cliente-side; o efeito é do servidor, o *gatilho* é nosso).

## Decisions

### D1 — "Learn & replay": o slot do inventário é **observado**, não adivinhado
**Decisão:** o módulo observa os pacotes de **saída** e, quando enxerga um `C1 06 26`,
decodifica o payload (Camada-2 inversa) e guarda os 6 bytes como **modelo**. Ele compara o
life/mana antes e depois do uso: se o **life** subiu, o modelo é de **HP**; se o **mana**
subiu, é de **MP**. O auto-potion passa a reenviar aquele modelo.

**Racional:** o `ObjectCharacter` do cliente (`Main_EX603/Main/Object.h`) tem
`pEquipment[12]` e o dinheiro, mas **não** tem o inventário — e o inventário do cliente vive
numa estrutura de UI que ninguém mapeou ainda. Sem o slot, o servidor ignora o pacote
(nada acontece, nada é consumido), então *adivinhar* o slot não dá nenhum dano, mas também
não dá função. Observar o pacote do próprio cliente é a única fonte de verdade sem RE nova:
o cliente já sabe o slot exato (flat do servidor) e o formato do payload.

**Alternativas consideradas (rejeitadas):**
- *Ler o inventário do cliente em memória* — exige achar a base da array + o mapeamento
  grid→slot do servidor (RE nova, endereço não documentado). Deixado para uma fase futura; o
  módulo já aceita slot manual para não depender disso.
- *Chamar a função do cliente que usa o item* — exige RE do caminho de UI e chamar código do
  jogo de outra thread (risco alto de crash).

### D2 — O pacote é **replayed verbatim**
**Decisão:** o modelo aprendido é o pacote de 6 bytes **já decodificado**; ao beber, o módulo
reaplica Camada-2 + Layer-1 e envia. Nada é "montado" a partir de palpite (inclusive o
`TargetSlot` e o `type`, que a gente não sabe o que o cliente manda).

**Racional:** o `type` só importa para Fruit e o `TargetSlot` precisa ser ≠ `SourceSlot`;
copiar o que o cliente mandou elimina as duas incertezas de graça.

### D3 — Porta de injeção: **um `case` a mais** no switch existente
**Decisão:** `PacketInjector::Inject` aceita `0x26` (Camada-2 [3,len) + Layer-1, sem SM).

**Racional:** a regra do projeto (skill §9) é "uma porta de injeção, um lugar que aplica
XorData". Criar `InjectItemUse` seria uma segunda porta com o mesmo contrato — exatamente o
que o refactor acabou de eliminar.

### D4 — `MuCipher::UnXorData` (inverso da Camada-2)
**Decisão:** adicionar o inverso no módulo de cripto (`UnXorData(data, start, end)`, mesma
conta, ordem **decrescente** — igual ao servidor).

**Racional:** sem isso não dá para ler o slot do pacote capturado: o que chega no sniffer é a
forma Camada-2 (o `FullDecrypt` do sniffer só desfaz a Layer-1). A Camada-2 **não** é
auto-inversa in-place (o byte anterior usado é o **cifrado**), então precisa da ordem
decrescente.

### D5 — Limiar e intervalo: **memória do cliente**, não do stream
**Decisão:** HP% e MP% vêm de `GameMemory::Player::Life()/MaxLife()` e `Mana()/MaxMana()`.

**Racional:** já é a fonte que o `AttackModule` usa; é leitura direta, sem depender de
timing de pacotes, e não precisa decodificar o life/mana update (C1 26/27) do servidor.

**Intervalo mínimo entre usos:** default **1000 ms**, configurável. O
`CheckAutoPotionHackTolerance` do servidor é desconhecido (0 = checagem desligada) e o
cliente MU tem delay de potion próprio (~500 ms) — 1000 ms é conservador e evita tomar
anti-hack.

### D6 — Módulo em `src/potion/`, header em `src/include/`
**Decisão:** `src/potion/AutoPotion.cpp` + `src/include/AutoPotion.h`, namespace `AutoPotion`,
comandos registrados no próprio `.cpp` via `REGISTER_MESSAGE` (mesmo padrão do
`AttackModule`/`DebugTools`).

**Racional:** segue o layout definido no refactor `reorganize-byp-modules` (§12 da skill):
um namespace = um arquivo, headers centralizados, `build.bat` com `SOURCES` explícito.

### D7 — O módulo **observa** o tráfego; o sniffer só **notifica**
**Decisão:** `PacketSniffer::OnReassembledPacket` detecta `C1 06 26` na direção de saída e
chama `AutoPotion::OnItemUsePacket(bytes, len)`. O módulo AutoPotion não conhece o sniffer
nem os hooks — só recebe o pacote decifrado (Camada-2) e o decodifica.

**Racional:** o sniffer já é o ponto único onde os pacotes são reensamblados e decifrados;
duplicar hook no AutoPotion seria acoplar o módulo de.feature ao de rede.

## Risks / Mitigations

| Risco | Mitigação |
|-------|-----------|
| Slot errado (potion não está no slot aprendido) | O servidor ignora (não consome, não pune). O `POTION_STATUS` mostra o slot aprendido; dá para reconfigurar (`POTION_CONFIG\|hpslot=..`). |
| Anti-hack do servidor (uso rápido demais) | Intervalo mínimo default 1000 ms + log de cada uso com o intervalo real. |
| O `XorData` do `C1 26` ser diferente do esperado | O modelo aprendido vem do **wire real do cliente**; o log mostra os bytes capturados e o replay usa exatamente eles. |
| Loop do watcher polling demais a memória | Tick de 100 ms, leitura de 4 WORDs — carga desprezível. |
| Thread do watcher chama o injetor (que usa o socket do GS) | O `PacketInjector` usa o socket do GS com o `send` original; o `AttackModule` já injeta de outras threads. |

## Open Questions

- O `TargetSlot`/`type` que o cliente manda para uma potion — será confirmado na primeira
  captura (o log mostra o pacote decodificado).
- O intervalo real de anti-hack do BlackRock — só o teste em campo revela; se o uso for
  rejeitado, o life/mana não sobe e o log mostra o pacote enviado.
- Se vale a pena mapear o inventário do cliente para descobrir as poções sozinho (fase
  futura) — depende do quanto o usuário usa de poções diferentes.
