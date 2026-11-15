# Skill C1 07 19 — Decodificação XOR + Correção do Pacote

Data: 2026-11-15

## Decodificação da captura (XorData / m_XorFilter)

Captura no wire (após Layer-1): `C1 07 19 E7 C3 E2 50`

O payload (bytes 3..6) passa por XOR encadeado — `PacketManager::XorData`
(MuEmu Main_EX603/PacketManager.cpp), tabela `m_XorFilter` (update ≥ 601:
`AB 11 CD FE 18 23 C5 A3 ...`). Decodificação de trás para frente:

    original[n] = cifrado[n] ^ cifrado[n-1] ^ key[n % 32]

| pos | campo  | cifrado | anterior | key    | resultado |
|-----|--------|---------|----------|--------|-----------|
| 3   | skillH | E7      | 19 (head)| FE     | 00        |
| 4   | skillL | C3      | E7       | 18     | 3C        |
| 5   | indexH | E2      | C3       | 23     | 02        |
| 6   | indexL | 50      | E2       | C5     | 77        |

Plaintext: `C1 07 19 00 3C 02 77`
- skill = 0x003C = 60 = SKILL_FORCE (Force do DL)
- index = 0x0277 = 631 (monstro — abaixo do limite de monstros)

**Conclusão:** o "skill wire 0xE7C3" era o plaintext `00 3C` (skill 60)
cifrado pelo XorData. NÃO é serial, NÃO é número de skill 0xE7C3.

## Formato real (S6)

    C1 07 19 [skillH] [skillL] [indexH] [indexL]   (7 bytes)

- skill primeiro, depois o entity index PURO (sem XOR 0xEFF5)
- sem direction byte (cliente envia 1 byte a menos que a struct do servidor;
  `CGSkillAttackRecv` só lê skill e index — SkillManager.cpp:5762/5803)
- `C1 07 19 E7 C3 E2 50` é a forma XorData'd (ANTES do SM), não o wire
- wire real da skill = C3 0D [11 bytes] (SM) — ver passo 02

## Mudanças de código

1. `AttackModule.cpp` — `SendSkillAttack`:
   - plaintext `[C1][07][19][skillH][skillL][indexH][indexL]` (skill primeiro)
   - entity index PURO (removido o XOR 0xEFF5)
   - `defaultSkillId`: 0xE7 (misread) → 0x003C (60 = Force)
   - removido `ComputeSkillDirection` (sem uso)
   - (o caminho de envio foi corrigido no passo 02: XorData → SM → C3)
2. `PacketSniffer.cpp` — `InjectC1Attack`: aceita opcode 0x19 além de 0x11
3. `MuOpcodes.h` / `AttackModule.h` / `PacketSniffer.h` — comentários atualizados
4. `MainForm.cs` (MuProxy2) — combo de skills:
   - `SkillOption.Action` = SkillNumber do F3 11 (o MESMO valor codificado no
     pacote; Force = 60 = 0x003C)
   - placeholder "Aguardando skills...": Action 0x1E (skill 30, inválida) → 0
     (byp usa defaultSkillId = Force)
   - removido campo morto `Dir` (0xCF) do record

## Validação da cadeia combo → wire

1. Servidor F3 11 (SkillManager.cpp:6382-6384):
   `info.skill = m_index` (2B LE), `info.level = (m_level<<3)|((m_index/255)&7)`
2. MuProxy2 lê SkillNumber (ex: 60) → combo → `SetSkill(0x003C)` →
   wire `skill=003C`
3. byp.dll: `g_SessionSkillId = 0x003C` → `plain[3]=0x00 plain[4]=0x3C`
4. Servidor: `skill = MAKE_NUMBERW(0x00,0x3C) = 60` → `GetSkill(Force)` ✓

## Build

- byp.dll: build.bat → SUCESSO, deployado em F:\blackrockmu\
- MuProxy2: dotnet build MuProxy2.csproj -c Release → 0 erros

## Pendente (validação em campo)

- Castar com o auto-attack skill ativo e conferir no log:
  `SkillAttack: plain=[C1 07 19 00 3C XX XX] -> InjectC1Attack`
  e o servidor aplicando dano de Force no monstro.
