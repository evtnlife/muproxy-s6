# Trace do Fluxo de Skill Attack — BlackRockMU

**Data:** 2026-08-10  
**Contexto:** Trace passo a passo do fluxo de skill attack a partir do breakpoint atingido em `main.exe+0x9DB688`.  
**Objetivo:** Entender como o jogo realmente monta, criptografa e envia o pacote de skill (Meteorite/Power Ball), para replicar corretamente no byp.dll.

---

## Estado Inicial (momento da pausa)

| Campo | Valor | Notas |
|-------|-------|-------|
| Address | `0019FF74` | Stack do jogo |
| To | `75B0FCC9` | Provável retorno para system/ntdll |
| From | `009DB688` | `main.exe+0x9DB688` — ponto de entrada do trace |
| Size | `0x10` | Tamanho da região/alloc |
| Party | System | |

### Breakpoints ativos no momento

| Type | Address | Module | State | Disassembly | Hits | Summary |
|------|---------|--------|-------|-------------|------|---------|
| Software | `6B90AD16` | main.dll | Enabled | `movss xmm0,dword ptr ds:[6BB2FBA8]` | 7 | |
| Software | `00404D70` | main.exe | Enabled | `push ebp` | 0 | SKILL_ENCRYPT_SEND (não bateu ainda) |
| Software | `004D34A4` | main.exe | Enabled | `add esp,4` | 1 | |
| Software | `004D957C` | main.exe | Enabled | `call main.565260` | 0 | |
| Software | `004DADA9` | main.exe | Enabled | `add esp,4` | 0 | |
| Software | `0059CEE6` | main.exe | Enabled | `mov dword ptr ss:[ebp-4],FFFFFFFF` | 0 | Skill packet builder (não bateu ainda) |
| Software | `005B14A7` | main.exe | Enabled | `add esp,8` | 0 | |
| Software | `005B5644` | main.exe | Enabled | `add esp,C` | 0 | |
| Software | `005B7745` | main.exe | Enabled | `add esp,4` | 0 | |
| Software | `009DB688` | main.exe | Enabled | `mov dword ptr ss:[ebp-20],eax` | 0 | **Breakpoint ativo agora** |

### Registers no momento da pausa

| Register | Valor | Interpretação |
|----------|-------|---------------|
| EAX | `0x0` | |
| EBX | `0x1` | |
| ECX | `0x19FF60` | Ponteiro para stack local |
| EDX | `0x0` | |
| ESI | `0x0` | |
| EDI | `0x47FD08` | Ponteiro para heap/object |
| EBP | `0x19FEE0` | Frame base |
| ESP | `0x19EE9C` | Stack pointer (muito distante de EBP — frame grande ou stack corrupto?) |
| EIP | `0x4D34A4` | **Ponto de execução atual** |
| EFLAGS | `0x395` | |

### Call Stack

| # | Endereço de retorno (stack) | From | To | Módulo/Função |
|---|-----------------------------|------|----|---------------|
| 0 | `0x19FEE4` | `0x4D34A4` | `0x9DB688` | `main.exe+0x4D34A4` |
| 1 | `0x19FF74` | `0x9DB688` | `0x75B0FCC9` | `main.exe+0x9DB688` |
| 2 | `0x19FF84` | `0x75B0FCC9` | `0x77CB82AE` | `kernel32.BaseThreadInitThunk+19` |
| 3 | `0x19FFE0` | `0x77CB82AE` | `0x77CB827E` | `ntdll.RtlGetAppContainerNamedObjectPath+11E` |
| 4 | `0x19FFF0` | `0x77CB827E` | `0x0` | `ntdll.RtlGetAppContainerNamedObjectPath+EE` |

### Disassembly próximo a `0x9DB688`

```asm
009DB688 | 89 45 E0 | mov dword ptr ss:[ebp-0x20], eax
009DB68B | 39 75 E4 | cmp dword ptr ss:[ebp-0x1C], esi
009DB68E | 75 06    | jnz 0x009DB696
009DB690 | 50       | push eax
009DB691 | E8 37 BD FF FF | call 0x009D73CD
009DB696 | E8 5E BD FF FF | call 0x009D73F9
009DB69B | 89 7D FC | mov dword ptr ss:[ebp-0x04], edi
009DB69E | EB 35    | jmp 0x009DB6D5
```

### Análise preliminar

- O EIP atual é `0x4D34A4`, que foi chamado por `0x9DB688`.
- `0x9DB688` foi chamado por `kernel32.BaseThreadInitThunk` — ou seja, estamos no topo de uma **thread worker/UI/timer**, não no main loop do jogo.
- Os breakpoints de skill (`0x404D70`, `0x59CEE6`) têm **0 hits** — este não é o fluxo de skill attack.
- A função `0x9DB688` parece ser um handler/exception/destructor (faz `mov [ebp-20], eax`, comparações, calls internos e jump para epílogo).

## Passo 1 — Parada em `main.exe+0x404D70` (SKILL_ENCRYPT_SEND)

Após dar `run`, o jogo parou no breakpoint `0x404D70`. Este **é** o fluxo de skill attack.

### Contexto da parada

| Campo | Valor | Interpretação |
|-------|-------|---------------|
| EIP | `0x404D70` | Início de `SKILL_ENCRYPT_SEND` |
| ECX | `0x19CB08` | Ponteiro para o struct de entrada (`this`) |
| EAX | `0x19CB08` | Mesmo valor de ECX (provável mov/copy) |
| EBP | `0x19D34C` | Frame base da função chamadora |
| ESP | `0x19B904` | Stack pointer |

### Call Stack no momento

| # | Stack | From | To | Função |
|---|-------|------|----|--------|
| 0 | `0x19B904` | `0x404D70` | `0x645C1D` | `main.exe+0x404D70` (atual) |
| 1 | `0x19D350` | `0x645C1D` | `0x6642AB` | `main.exe+0x645C1D` |
| 2 | `0x19D4A0` | `0x6642AB` | `0x6BA2FA4F` | `main.exe+0x6642AB` |
| 3 | `0x19D630` | `0x6BA2FA4F` | `0x65FD7E` | `main.XDrawElementIco+3ADF` (Main.dll!) |
| 4 | `0x19EE90` | `0x65FD7E` | `0x4D3575` | `main.exe+0x65FD7E` |
| 5 | `0x19FEE4` | `0x4D3575` | `0x9DB688` | `main.exe+0x4D3575` |
| 6 | `0x19FF74` | `0x9DB688` | `0x75B0FCC9` | `main.exe+0x9DB688` |
| 7 | `0x19FF84` | `0x75B0FCC9` | `0x77CB82AE` | `kernel32.BaseThreadInitThunk+19` |

**Observação:** A skill não vem de `0x59CEE6` (builder antigo), mas de uma cadeia que passa por `0x645C1D` e `Main.dll` (`XDrawElementIco+3ADF`). O endereço `0x59CEE6` provavelmente é de outro tipo de skill ou build antigo.

### Struct de entrada em `ECX = 0x19CB08`

Lendo 32 bytes a partir de `0x19CB08`:

```
06 00 C1 01 F1 0C 14 2E 85 00 05 27 00 00 00 85
00 00 00 27 05 00 00 C1 10 27 FF 02 7A 00 6D 7A
```

Interpretação inicial:
- `[ecx]` (word) = `06 00` → tamanho = **6** (tamanho do payload, não inclui header?)
- `[ecx+2]` (buf) começa com `C1 01 F1 0C 14 2E ...`

O primeiro byte do buffer é `C1` — isso contradiz a ideia de que o plaintext skill começa com `C3`. O formato parece ser:
```
[word size=6][C1 01 F1 0C 14 2E ...]
```

Disassembly de `0x404D70`:
```asm
00404D70 | 55       | push ebp
00404D71 | 8B EC    | mov ebp, esp
00404D73 | 51       | push ecx
00404D74 | 89 4D FC | mov [ebp-4], ecx     ; salva this
00404D77 | 8B 4D FC | mov ecx, [ebp-4]
00404D7A | E8 21 02 00 00 | call 0x404FA0   ; init/serial?
00404D7F | 8B 45 0C | mov eax, [ebp+C]     ; arg2
00404D82 | 50       | push eax
00404D83 | 8B 4D 08 | mov ecx, [ebp+8]     ; arg1
00404D86 | 51       | push ecx
00404D87 | 8B 55 FC | mov edx, [ebp-4]
00404D8A | 0F B7 02 | movzx eax, word [edx] ; size = [ecx]
00404D8D | 50       | push eax              ; size
00404D8E | 8B 4D FC | mov ecx, [ebp-4]
00404D91 | 83 C1 02 | add ecx, 2            ; buffer = ecx+2
00404D94 | 51       | push ecx              ; buffer
00404D95 | E8 16 00 00 00 | call 0x404DB0   ; encrypt
00404D9A | 83 C4 10 | add esp, 10
00404D9D | 8B E5    | mov esp, ebp
00404D9F | 5D       | pop ebp
00404DA0 | C2 08 00 | ret 8
```

A função `0x404D70` é `stdcall` (limpa 8 bytes na stack), não `__thiscall`. Ela recebe:
- `[ebp+08]` = arg1
- `[ebp+0C]` = arg2
- ECX = struct { word size; byte buf[]; }

## Passo 2 — Após `call 0x404FA0` (init/set size)

Após executar `call 0x404FA0` em `0x404D7A`:

| Register | Valor | Nota |
|----------|-------|------|
| EAX | `0x19CB0A` | ECX + 2 (ponteiro para buffer) |
| ECX | `0x19CB08` | Struct original |
| EDX | `0x3` | Retorno da função init? |

### Buffer em `0x19CB08` após init

Antes do call:
```
06 00 C1 01 F1 0C 14 2E 85 00 05 27 00 00 00 85
```

Depois do call:
```
06 00 C1 06 F1 0C 14 2E 85 00 05 27 00 00 00 85
```

**Mudança crítica:** o byte em `[ecx+3]` mudou de `01` para `06`.  
Isso confirma que `0x404FA0` é responsável por **setar o tamanho real do pacote** no segundo byte (logo após o header `C1`).

### Interpretação do struct de entrada

| Offset | Valor | Significado |
|--------|-------|-------------|
| `[ecx]` | `06 00` | Word = 6 (tamanho do payload após o header?) |
| `[ecx+2]` | `C1` | Header do pacote |
| `[ecx+3]` | `06` | Size byte (setado por 0x404FA0) |
| `[ecx+4]` | `F1` | Opcode? |
| `[ecx+5..]` | `0C 14 2E 85 00 05 ...` | Payload restante |

**Nota:** Nosso código atual envia `C1 01 19 ...` e espera que `0x404FA0` substitua o `01` pelo size. Pelo visto, o byte seguinte ao header é mesmo o size. Mas o nosso opcode é `0x19` (TargetedSkill), enquanto aqui o opcode é `0xF1`. Isso pode indicar outro tipo de skill ou outro opcode para Meteorite/Power Ball.

## Passo 3 — Preparando argumentos para `0x404DB0` (encrypt)

Disassembly atual em `0x404D7F`:
```asm
00404D7F | 8B 45 0C | mov eax, [ebp+C]   ; arg2
00404D82 | 50       | push eax
00404D83 | 8B 4D 08 | mov ecx, [ebp+8]   ; arg1
00404D86 | 51       | push ecx
00404D87 | 8B 55 FC | mov edx, [ebp-4]   ; this
00404D8A | 0F B7 02 | movzx eax, word [edx] ; size = [ecx] = 6
00404D8D | 50       | push eax           ; size
00404D8E | 8B 4D FC | mov ecx, [ebp-4]
00404D91 | 83 C1 02 | add ecx, 2         ; buffer = ecx+2
00404D94 | 51       | push ecx           ; buffer
00404D95 | E8 16 00 00 00 | call 0x404DB0 ; encrypt
```

Argumentos para `0x404DB0`:
- Buffer: `0x19CB0A` (ECX+2)
- Size: `6`
- arg1 (empilhado primeiro): `[ebp+08]`
- arg2: `[ebp+0C]`

## Passo 4 — Saída de `0x404D70` e chegada em `0x4D957C` (main loop)

O step over saltou de `0x404D70` direto para `0x4D957C`. Isso significa que `0x404D70` executou até o `ret 8`, passando por `0x404DB0` (encrypt) e `0x404DD1` (send) internamente.

### Estado em `0x4D957C`

| Register | Valor | Nota |
|----------|-------|------|
| EIP | `0x4D9581` | Após `call 0x565260` em `0x4D957C` |
| EAX | `0x3E8` | 1000 (decimal) — possível tick/time |
| EBP | `0x19E4B8` | Frame do main loop |
| ESP | `0x19DC60` | Stack do main loop |
| ECX | `0x974BA68` | Ponteiro de heap |
| ESI | `0x9F6E620` | |
| EDI | `0x850132B5` | |

### Call Stack

| # | Stack | From | To | Função |
|---|-------|------|----|--------|
| 0 | `0x19E4BC` | `0x4D957C` | `0x6B90AD16` | `main.exe+0x4D957C` |
| 1 | `0x19E670` | `0x6B90AD16` | `0x4DADA9` | `main.dll+0xAD16` (XDrawElementIco+?) |
| 2 | `0x19EE98` | `0x4DADA9` | `0x4D34A4` | `main.exe+0x4DADA9` |
| 3 | `0x19FEE4` | `0x4D34A4` | `0x9DB688` | `main.exe+0x4D34A4` |
| 4 | `0x19FF74` | `0x9DB688` | `0x75B0FCC9` | `main.exe+0x9DB688` |

### O que aconteceu com o buffer?

A memória `0x19CB08` agora contém dados diferentes:
```
20 B2 D4 0C C8 DB 19 00 80 91 CD 00 FC 49 C9 0F
24 CB 19 00 32 86 4C 00 F8 49 C9 0F 40 CB 19 00
```

Isso não é mais o nosso struct de skill — a memória foi reutilizada. Para capturar o pacote criptografado que saiu, precisaríamos ter logado na saída de `0x404D70` ou ter lido o buffer logo após `0x404DB0`.

### Disassembly de `0x4D957C`

```asm
004D957C | E8 DF BC 08 00 | call 0x00565260
004D9581 | E8 AA 1C 00 00 | call 0x004DB230
004D9586 | 8B C8          | mov ecx, eax
004D9588 | E8 23 BD 44 00 | call 0x009252B0
004D958D | E8 5E B5 4A 6B | call 0x6B984AF0   ; Main.dll
004D9592 | E8 F9 19 27 00 | call 0x0074AF90
```

Este é o main loop do jogo. A skill foi disparada durante uma iteração deste loop.

## Passo 5 — Parada em `main.dll+0x1AD16` (`0x6B90AD16`)

Após novo `run`, o debugger parou em `0x6B90AD16`, dentro de `main.dll`.

### Informações do módulo

| Campo | Valor |
|-------|-------|
| Base | `0x6B8F0000` |
| Offset do breakpoint | `0x1AD16` |
| Nome | `main.dll` |

### Contexto da parada

| Register | Valor |
|----------|-------|
| EAX | `0xEBB290` |
| EBX | `0x482B70` |
| ECX | `0x19EE88` |
| EDX | `0x97786C4` |
| ESI | `0x9F6E620` |
| EDI | `0x850132B5` |
| EBP | `0x19E66C` |
| ESP | `0x19E4C0` |
| EIP | `0x6B90AD16` |

### Disassembly

```asm
6B90AD16 | F3 0F 10 05 A8 FB B2 6B | movss xmm0, dword ptr ds:[0x6BB2FBA8]
6B90AD1E | B8 F0 FD 51 00          | mov eax, 0x51FDF0
6B90AD23 | F3 0F 59 05 6C 3F B2 6B | mulss xmm0, dword ptr ds:[0x6BB23F6C]
6B90AD2B | 51                      | push ecx
6B90AD2C | B9 5C CB E8 00          | mov ecx, 0xE8CB5C
6B90AD31 | F3 0F 11 04 24          | movss dword ptr ss:[esp], xmm0
6B90AD36 | FF D0                   | call eax          ; call 0x51FDF0
6B90AD38 | B8 A0 77 59 00          | mov eax, 0x5977A0
6B90AD3D | FF D0                   | call eax          ; call 0x5977A0
6B90AD3F | 6A 2C                   | push 0x2C
```

### Interpretação

- `0x51FDF0` e `0x5977A0` são chamadas indiretas via registrador.
- `0x5977A0` é um endereço em `main.exe` (base `0x400000` + `0x1977A0`).
- A função `0x51FDF0` pode ser um timer/tick handler.
- O fluxo de skill parece ser: `main.dll` (loop/render) → `main.exe` (`0x4DADA9` → `0x4D34A4`) → eventualmente `0x404D70`.

## Passo 6 — Parada em `main.exe+0x4DADA9` (Builder da Skill)

Esta é a parada mais importante até agora. O debugger parou no builder real do pacote skill.

### Disassembly do builder

```asm
004DADA9 | 83 C4 04       | add esp, 4
004DADAC | EB 02          | jmp 0x004DADB0
004DADAE | FF 15 833DA4C5 | call dword ptr ds:[0xC5A43D83]
004DADB4 | E8 001F7E70    | call 0x70CBCCB9
004DADB9 | 8D 8D EC F7 FF FF | lea ecx, ss:[ebp-0x814]
004DADBF | E8 7C 9D F2 FF | call 0x00404B40
004DADC4 | C7 45 FC 00 00 00 00 | mov dword ptr ss:[ebp-0x04], 0x00
004DADCB | 68 F1 00 00 00 | push 0xF1
004DADD0 | 68 C1 00 00 00 | push 0xC1
004DADD5 | 8D 8D EC F7 FF FF | lea ecx, ss:[ebp-0x814]
004DADDB | E8 B0 9D F2 FF | call 0x00404B90
004DADE0 | 6A 00          | push 0x00
004DADE2 | 6A 02          | push 0x02
004DADE4 | 6A 03          | push 0x03
004DADE6 | 8D 8D EC F7 FF FF | lea ecx, ss:[ebp-0x814]
004DADEC | E8 DF A2 F2 FF | call 0x004050D0
004DADF1 | 8B C8          | mov ecx, eax
004DADF3 | E8 D8 A2 F2 FF | call 0x004050D0
004DADF8 | 8B C8          | mov ecx, eax
004DADFA | E8 D1 A2 F2 FF | call 0x004050D0
004DADFF | 6A 00          | push 0x00
004DAE01 | 6A 01          | push 0x01
004DAE03 | 8D 8D EC F7 FF FF | lea ecx, ss:[ebp-0x814]
004DAE09 | E8 62 9F F2 FF | call 0x00404D70    ; SKILL_ENCRYPT_SEND
004DAE0E | C7 45 FC FF FF FF FF | mov dword ptr ss:[ebp-0x04], 0xFFFFFFFF
004DAE1B | E8 40 9D F2 FF | call 0x00404B60    ; provável destructor
004DAE36 | C3             | ret
```

### Construção do pacote

O builder faz o seguinte:
1. Inicializa um objeto em `[ebp-0x814]` via `0x404B40`.
2. Chama `0x404B90` com args `(0xF1, 0xC1)` e ECX = `[ebp-0x814]` — provavelmente seta header e opcode.
3. Chama `0x4050D0` três vezes com args `(0x00, 0x02, 0x03)` — provavelmente escreve bytes no buffer.
4. Chama `0x4050D0` mais duas vezes encadeadas (retorna ponteiro, passa adiante) — escreve mais bytes.
5. Finalmente chama `0x404D70` com args `(0x00, 0x01)` e ECX = `[ebp-0x814]`.

### Argumentos para `0x404D70`

```asm
push 0x00    ; arg2
push 0x01    ; arg1
lea ecx, [ebp-0x814]
call 0x00404D70
```

Portanto, na nossa chamada para `0x404D70` precisamos passar:
- `ECX` = ponteiro para o struct/buffer
- `[ESP]` = `0x01`
- `[ESP+4]` = `0x00`

### Estado do struct em `[ebp-0x814]`

EBP atual = `0x19EE94`, então `[ebp-0x814]` = `0x19E680`.

Lendo 32 bytes a partir de `0x19E680`:
```
00 00 00 00 00 00 00 00 00 00 00 00 0F 00 00 C0
AC F4 8B 1F 97 E7 19 00 00 00 40 00 01 00 00 00
```

Isso não parece um buffer linear de bytes. Parece um objeto C++ com vtable/ponteiro no início. O pacote provavelmente é construído internamente por `0x404B90` e `0x4050D0`, e o objeto mantém um buffer dinâmico em algum offset interno.

### Próximo passo

Vou setar um breakpoint em `0x4DAE09` (o `call 0x404D70` dentro do builder) para capturar o buffer completo no momento exato antes da criptografia/envio.







