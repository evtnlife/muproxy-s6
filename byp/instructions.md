# BlackRockMU — Instruções do Projeto

> **Regra de ouro:** quem decripta é o `byp.dll`. O MuProxy2 (C#) **nunca** toca em criptografia.
> Se você sentir vontade de adicionar `Decrypt`/`XorKey`/`Encrypt` no lado C#, **pare** — o design está errado.

## Onde está o conhecimento

O conhecimento consolidado do projeto (arquitetura, canais TCP, criptografia
Layer-1/XorData/skill, pacote de ataque validado, object array, offsets, build/deploy)
está na skill **`blackrockmu`**. **Carregue-a antes de editar código** em `gg_bypass2/` ou `MuProxy2/`.

Fontes de referência (não auto-carregadas):
- `byp/docs/attack_packet_re.md` — RE validado do pacote de ataque (fonte de verdade).
- `byp/docs/` — traces detalhados (skill_trace, ATTACK_PACKET_ANALYSIS, crypto, plan-debugC3).
- `OpenMU-master/docs/Packets/` — estruturas/opcodes de pacotes autoritativas.
- `MuEmu-master/` — source do emulador (server-side). **Versão em uso: `ex603`**
  (`Client_EX603` + `Main_EX603`). Todos os offsets hardcoded dependem desse build.

## Estrutura

```
F:\blackrockmu\
  ├─ byp.dll, opengl32.dll, MuProxy2.exe   (deploy na pasta do jogo)
  ├─ byp\
  │    ├─ gg_bypass2\                       (C++: byp.dll + opengl proxy)
  │    ├─ MuProxy2\                         (C# .NET 8 WinForms)
  │    └─ docs\                             (referências RE)
  ├─ OpenMU-master\                         (docs de pacotes)
  └─ MuEmu-master\                          (source do emulador; usamos ex603)
```

## Regras de ouro (anti-gambiarra)

1. **Criptografia é do byp.dll (C++).** MuProxy2 nunca decripta/encripta.
2. **Mensagens usam pipe `|`**, nunca espaço. Formato `TAG|payload`.
3. **`TcpLoopbackProxy` é relay cego** — não inspeciona, não decripta, não modifica.
4. **Packet List é alimentada pelo canal 44413 (PKT)**, não pelo proxy.
5. **Novas mensagens** precisam de `[MessageTag]` + `Parse()` estático.
6. **Endereços hardcoded** (`GameAddresses.h`) são específicos do build **ex603** —
   re-valide todos os offsets se o binário mudar.
7. **XorData (Camada-2) É aplicado** aos bytes de dados do ataque — não pule.
8. **Skill 0xE5 usa criptografia própria** — não reutilize a Layer-1 para skills.
9. **Teste o byp.dll standalone primeiro** (só ataque automático); MuProxy2 é camada extra.
