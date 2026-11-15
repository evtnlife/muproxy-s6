# Diferença C3/C1 0x19 — MuProxy (manual) vs UI do jogo

**Data:** 2026-11-15
**Contexto:** Mesmo personagem, mesma skill (Force/DL, F3 11 id = `0x003C`), mesmo monstro,
mesma posição. Ataque disparado via MuProxy (MANUAL) às 05:18 e via UI do jogo às 05:21.

---

## 1. Os dois pacotes (plaintext decifrado, pós-SM)

| | MuProxy MANUAL (nós) | UI do jogo |
|---|---|---|
| Wire C3 | `C3 0D ...` (13 bytes) | `C3 0D A5 2B 60 14 0F 21 E3 8B E8 5A 61` (13 bytes) |
| **Plaintext** | `C1 07 19 ED 00 17 3C` | `C1 07 19 E7 C3 E2 C5` |

Prefixo idêntico: `C1 07 19`. Divergência nos 4 bytes de payload:

| Posição | Nós | Jogo |
|---|---|---|
| byte 3 | `ED` | `E7` |
| byte 4 | `00` | `C3` |
| byte 5 | `17` | `E2` |
| byte 6 | `3C` | `C5` |

## 2. Como cada um foi montado

**Nós** (`AttackModule.cpp:257-264`) — formato **intercalado**, copiado do header eMU
`PMSG_SKILL_ATTACK_RECV` (GAMESERVER_UPDATE>=701):

```
C1 07 19 [idxH] [skillH] [idxL] [skillL]
         ED     00       17     3C
idx wire   = 0xED17 = entityIndex(0x02E2) ^ 0xEFF5
skill wire = 0x003C (id bruto da lista F3 11 do Force)
```

**Jogo** — evidência acumulada de capturas reais:

| # | Captura real (payload pós-`C1 07 19`) | Skill | Target wire → raw (^0xEFF5) |
|---|---|---|---|
| 1 | `E7 EE CF 12` (comentário AttackModule.cpp:126) | wire=0xE7EE (?) | 0xCF12 → 0x20E7 |
| 2 | `E7 C3 E2 CF` (doc + comentário :208-210, trace x64dbg anterior) | **wire=0xE7C3** (Force) | 0xE2CF → 0x0D3A |
| 3 | `E7 C3 E2 C5` (**hoje, UI**) | **wire=0xE7C3** (Force) | 0xE2C5 → 0x0D30 |

### Argumento da constância (decide o layout)

As capturas #2 e #3 são dois casts de **mesma skill (Force)**:
- Bytes 3-4 (`E7 C3`) **idênticos** nos dois casts → campo constante por skill.
- Byte 6 varia (`CF`→`C5`) → acompanha o alvo, não a skill.

Se o layout fosse o intercalado nosso, as capturas #2/#3 dariam skills `0xC3CF` e `0xC3C5` —
**não constantes para a mesma skill** (impossível). No layout **não-intercalado**, os dois
casts dão exatamente a mesma skill wire (`0xE7C3`). Logo:

```
FORMATO REAL DO JOGO:  C1 07 19 [skillH] [skillL] [targetH] [targetL]
                        E7       C3       E2       C5
skill wire   = 0xE7C3 (constante por skill — Force)
target wire  = entityIndex ^ 0xEFF5
```

Isso bate com o comentário em `AttackModule.cpp:200-214` ("FORMATO CORRETO confirmado via
trace x64dbg — 4 addData calls"), que a implementação atual **desobedece** (alguém trocou para
o formato intercalado do eMU achando que o header do servidor era a fonte da verdade).

## 3. As três diferenças concretas

1. **Layout**: jogo = `[skillH][skillL][targetH][targetL]` (contíguo); nós = intercalado
   `[idxH][skillH][idxL][skillL]`. → O nosso pacote, lido pelo servidor real do BlackRock,
   decodifica skill/target errados.

2. **Valor da skill**: o jogo envia `0xE7C3` para o Force; nós enviamos `0x003C` (id F3 11).
   O campo "skill" do C1 07 19 **não é** o id da lista F3 11 — é outro identificador
   (provavelmente número interno da tabela de skills do cliente, ou derivado dele).
   **RE pendente**: localizar de onde sai `E7 C3` no builder.

3. **Valor do target**: hoje o jogo usou raw = `0xE2C5 ^ 0xEFF5 = 0x0D30`; nós lemos
   `entityIndex = 0x02E2` (OFF_INDEX) para "o mesmo monstro". Ou o monstro foi re-indexado
   entre 05:18 e 05:21, ou nosso OFF_INDEX lê campo errado. **RE pendente**: confirmar qual
   campo do objeto o builder copia para targetH/targetL e se a chave `0xEFF5` é constante.

## 4. Observações laterais

- **Direction**: calculamos `ComputeSkillDirection` (dir=0x80 no log) mas **não** colocamos no
  pacote — e o pacote do jogo também tem apenas 7 bytes, sem lugar para direction. OK assim.
- **Tamanho/criptografia**: ambos C3 com payload SM de 11 bytes → decLen 6+header; nosso
  caminho de criptografia (SM + Layer-1) está correto — a diferença é só o plaintext.
- **Stack do send (UI, hoje)**: `... main.exe+0x19BA17 → Main.dll+0x1484F1 → main.exe+0x19C7A7
  → main.exe+0x4D9A/0x4EF9/0x5153 → Main.dll+0xBB3DE (send)`. O builder do C1 07 19 mora na
  função que contém o retorno `main.exe+0x19C7A7` (absoluto ≈ `0x59C7A7`).

## 5. Perguntas para a sessão x32dbg

| # | Pergunta | Como responder |
|---|---|---|
| Q1 | De onde saem os bytes `[skillH][skillL]` (`E7 C3`)? Campo de qual objeto/tabela? | BP no builder; inspecionar o valor-fonte (registrador/memória) e rastrear até a tabela de skills |
| Q2 | Qual campo do objeto-alvo vira `targetH targetL`, e a chave XOR é sempre `0xEFF5`? | BP no builder com alvo conhecido; comparar bytes do pacote vs OFF_INDEX/OFF_ID/slot do monstro |
| Q3 | O layout é mesmo contíguo `[skill][target]` (e não intercalado)? | Ver as 4 escritas de byte na ordem em que acontecem |

**Plano:** `init F:\blackrockmu\main.exe` no x32dbg → desmontar a região de `0x59C7A7`,
localizar o início da função e as escritas do buffer → BP antes do send → usuário loga e
casta Force num monstro conhecido → capturar registros/memória → responder Q1-Q3.
