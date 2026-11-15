# Skill C1 07 19 — Pipeline C3 (correção do passo 01)

Data: 2026-11-15

## Correção

O passo 01 mandou a skill como **C1 puro** (XorData + Layer-1, igual ao 0x11).
ERRADO. O caminho real da skill (informado pelo usuário, confere com as
capturas) é:

    C1 07 19 [payload]
      ─► XorData[3,7) (XOR encadeado, anchor 0x19, key m_XorFilter)
      ─► SM input = [serial 0x08793700] + payload XorData'd (6 bytes)
      ─► SimpleModulus (6 → 11 bytes, 1 bloco)
      ─► C3 0D [11 bytes]
      ─► Layer-1 (XOR+sub 0x36/0x07)
      ─► wire

**Só a skill (0x19)** faz esse caminho. O ataque normal 0x11 continua
C1 puro (XorData + Layer-1).

A captura `C1 07 19 E7 C3 E2 50` (passo 01) é a forma **XorData'd, ANTES do
SM** — não é o wire. O wire da skill é o `C3 0D [11]` que aparecia nas
capturas.

## Mudanças de código

1. `PacketSniffer.cpp` — nova função `InjectSkillC1(plaintext, len)`:
   - valida C1/0x19/7 bytes
   - `GameServerXorData(buf, 3, 7)`
   - SM input = `[*0x08793700] + buf[2..6]` (serial lido e incrementado)
   - `SimpleModulusCipher::Encrypt` (6 → 11)
   - monta `C3 0D [11]`
   - `InjectToServer` (Layer-1 + send)
2. `PacketSniffer.h` — declaração de `InjectSkillC1`
3. `AttackModule.cpp` — `SendSkillAttack` agora chama
   `PacketSniffer::InjectSkillC1(plain, 7)` (antes `SendPlainPacket`)
4. `AttackModule.h` — comentário do pipeline atualizado

## O que ficou do caminho MANUAL antigo (fila)

O `SKILL_TYPE_MANUAL` do `ProcessSkillQueue` (SM → C3) tinha a estrutura SM
certa ([serial]+payload → 11 → C3 0D), mas o layout era intercalado, usava
0xEFF5 e NÃO aplicava XorData antes do SM. Agora o skill não passa mais pela
fila — vai direto por `InjectSkillC1` (nossa SM é C++ próprio, roda em
qualquer thread). A fila ficou como legado.

## Build

- byp.dll: build.bat → SUCESSO, deployado em F:\blackrockmu\

## Pendente (validação em campo)

- Castar com auto-attack skill ativo e conferir no log:
  - `SkillAttack: plain=[C1 07 19 00 3C XX XX] -> InjectSkillC1`
  - `InjectSkillC1: after XorData [C1 07 19 E7 C3 ...]`
  - `InjectSkillC1: sent C3 0D (13 bytes) skill=0x003C index=0xXXXX`
- Se o monstro tomar dano de Force, pipeline confirmado.
