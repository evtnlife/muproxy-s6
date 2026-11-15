# Análise do Pacote de Ataque — MU Online BlackRock

## Resumo Executivo

Pacotes de ataque enviados pela DLL `byp.dll` estavam com **dois bytes incorretos**:
1. **Action byte**: era `0x0A`, correto é `0x04` (já corrigido)
2. **Direction byte**: era hardcoded `0xCF` ou `0xC0`, deve ser `0xC0 | dir` onde `dir` é calculado via `atan2` (parcialmente corrigido — `FindObjectByEntityIndex` adicionada mas precisa de validação em runtime)

O pacote de ataque real capturado do cliente oficial é: `C1 07 11 ED 5F 04 C5`

---

## Formato do Pacote de Ataque Normal (0x11)

| Offset | Bytes | Campo      | Descrição                                    |
|--------|-------|------------|----------------------------------------------|
| 0      | C1    | Header     | C1 = pacote simples (sem加密)                |
| 1      | 07    | Size       | Tamanho total em bytes                        |
| 2      | 11    | Opcode     | 0x11 = ataque normal                         |
| 3      | ED    | TargetHi   | Byte alto do Attack Target ID (big-endian)   |
| 4      | 5F    | TargetLo   | Byte baixo do Attack Target ID               |
| 5      | 04    | Action     | 0x04 = ataque normal (confirmado via RE)     |
| 6      | C5    | Direction  | 0xC0 \| dir, onde dir = 0-7 (8 direções)   |

### Attack Target ID

```
AttackTargetID = EntityIndex XOR 0xEFF5
```

- **EntityIndex** = campo `OFF_INDEX` (offset 0x07E) no objeto, formato WORD
- **XOR Key** = `0xEFF5` (descoberto via engenharia reversa, confirmado em múltiplas sessões)
- Exemplo: EntityIndex `0x0292` → AttackTargetID = `0x0292 ^ 0xEFF5 = 0xED67`

---

## Cálculo da Direção (Direction Byte)

### Descoberta Principal

A direção **NÃO** é armazenada como um byte simples no objeto do jogador. Ela é **calculada dinamicamente** a partir das coordenadas X/Y do player e do alvo, usando `atan2()`.

### Offsets de Coordenadas (no Object Structure)

| Offset | Tipo   | Campo          | Descrição                          |
|--------|--------|----------------|------------------------------------|
| 0x404  | float  | OFF_X          | Posição X do objeto (centésimos de tile) |
| 0x408  | float  | OFF_Y          | Posição Y do objeto (centésimos de tile) |
| 0xFC   | float  | —              | Posição X do player (usado na função de ataque) |
| 0x100   | float  | —              | Posição Y do player (usado na função de ataque) |
| 0xD0   | float  | —              | Posição X do target (usado na função de ataque) |
| 0xD4   | float  | —              | Posição Y do target (usado na função de ataque) |
| 0x110  | float  | OFF_DIR_ANGLE  | Ângulo de direção (float, populado APENAS durante ataque) |

**Nota**: Os offsets 0xFC, 0x100, 0xD0, 0xD4 são usados pela função de ataque do Main.exe mas podem apontar para sub-estruturas diferentes. Os offsets 0x404/0x408 são os confirmados para a nossa DLL.

### Função de Ataque no Main.exe (endereços-chave)

| Endereço   | Instrução                                      | Descrição                               |
|------------|-------------------------------------------------|-----------------------------------------|
| 0x5AC27C  | `fld dword ptr ds:[eax+0xD4]`                | Carrega target Y (offset 0xD4)         |
| 0x5AC289  | `fld dword ptr ds:[ecx+0xD0]`                | Carrega target X (offset 0xD0)          |
| 0x5AC296  | `fld dword ptr ds:[edx+0x100]`               | Carrega player Y (offset 0x100)         |
| 0x5AC2A3  | `fld dword ptr ds:[eax+0xFC]`                | Carrega player X (offset 0xFC)          |
| 0x5AC2AC  | `call 0x540C30`                               | Calcula atan2(targetY-playerY, targetX-playerX) |
| 0x5AC2B7  | `fstp dword ptr ds:[ecx+0x110]`              | Salva ângulo em [player+0x110]          |
| 0x5AC370  | `fadd qword ptr ds:[0x00D27CC0]`             | +180.0 (normaliza ângulo negativo)      |
| 0x5AC381  | `fdiv qword ptr ds:[0x00D27CB8]`             | /360.0                                  |
| 0x5AC387  | `fmul qword ptr ds:[0x00D22A88]`             | *255.0                                  |
| 0x5AC3AB  | `fistp dword ptr ss:[ebp-0x5A1C]`            | Converte para int → direction_byte_1    |
| 0x5AC3BD  | `mov byte ptr ss:[ebp-0x4145], al`           | Armazena direction_byte_1                |
| 0x5AC43F  | `push 0xC1` / `push 0x07`                    | Header C1, tamanho 07                   |
| 0x5AC45C  | `push edx` (target ID low byte)              |                                          |
| 0x5AC467  | `push eax` (target ID high byte)             |                                          |
| 0x5AC468  | `movzx ecx, byte ptr ss:[ebp-0x4145]`       | Carrega direction_byte_1                 |
| 0x5AC470  | `movzx edx, byte ptr ss:[ebp-0x417D]`       | Carrega action byte                       |
| 0x5AC47B  | `fld dword ptr ds:[eax+0x110]`              | Carrega ângulo do player (offset 0x110)   |
| 0x5AC481  | `fdiv qword ptr ds:[0x00D27CB8]`             | /360.0                                  |
| 0x5AC487  | `fmul qword ptr ds:[0x00D22A88]`             | *255.0                                  |
| 0x5AC4AB  | `fistp dword ptr ss:[ebp-0x5A28]`            | Converte para int → direction_byte_2     |
| 0x5AC4B7  | `movzx ecx, byte ptr ss:[ebp-0x5A28]`       | Carrega direction_byte_2                  |
| 0x5AC4BE  | `push ecx` (direction byte)                  | **Este é o byte que vai no pacote**      |
| 0x5AC4C2  | `movzx eax, byte ptr ds:[edx+0xB0]`         | Target offset 0xB0                       |
| 0x5AC4CD  | `movzx edx, byte ptr ds:[ecx+0xAC]`          | Target offset 0xAC                       |

### Constantes FPU Usadas

| Endereço   | Valor (double) | Descrição           |
|------------|-----------------|---------------------|
| 0xD27CC0  | 180.0           | Offset de normalização |
| 0xD27CB8  | 360.0           | Divisor              |
| 0xD22A88  | 255.0           | Multiplicador        |

### Dois Cálculos de Direção

O Main.exe calcula a direção **duas vezes**:

1. **Direction_byte_1** (`[ebp-0x4145]`): `(angle + 180.0) / 360.0 * 255.0` → trunca para int
2. **Direction_byte_2** (`[ebp-0x5A28]`): `angle / 360.0 * 255.0` → trunca para int

O byte que vai no pacote é **direction_byte_2** (o segundo cálculo, SEM o +180).

### Fórmula Implementada na DLL

```cpp
// Fórmula MU Online (Y cresce para baixo na tela):
// dir = floor((atan2(dy, dx) + PI/2) / (2*PI) * 8) % 8
// Resultado: N=0, NE=1, E=2, SE=3, S=4, SW=5, W=6, NW=7
float angle = atan2f(dy, dx);
float dir = floorf((angle + PI * 0.5f) / (2.0f * PI) * 8.0f);
int dirInt = (int)dir % 8;
if (dirInt < 0) dirInt += 8;
return (uint8_t)(0xC0 | dirInt);
```

**Verificação**: Alvo SW (dx<0, dy>0):
- `atan2(+,−) ≈ 2.356 rad`
- `(2.356 + 1.571) / 6.283 * 8 = 5.0`
- `dir = 5`
- `0xC0 | 5 = 0xC5` ✅ (confirma com pacote real)

---

## Estrutura do Objeto (Object Structure) — Offsets Confirmados

Tamanho do slot: **0x598 bytes** (1432)
Máximo de objetos: **400**

| Offset  | Tipo   | Campo       | Descrição                                    | Status      |
|---------|--------|-------------|----------------------------------------------|-------------|
| 0x000   | DWORD  | vtbl        | Vtable pointer                               | ✅ Confirmado |
| 0x038   | char[] | NAME         | Nome do objeto (string)                       | ✅ Confirmado |
| 0x07E   | WORD   | OFF_INDEX   | Composite Entity Index (XOR key 0xEFF5)      | ✅ Confirmado |
| 0x070   | BYTE   | OFF_DIR     | ❌ Sempre 0 — direção NÃO é armazenada aqui   | ❌ Inválido  |
| 0x110   | float  | OFF_DIR_ANGLE | Ângulo de direção (float, só durante ataque) | ⚠️ Zero quando idle |
| 0x19C   | short  | LEVEL       | Level (real = stored − 0x493)                 | ✅ Confirmado |
| 0x208   | short  | LEVEL2      | Level alternativo                             | ✅ Confirmado |
| 0x29C   | DWORD  | INTPTR      | Internal object pointer                       | ✅ Confirmado |
| 0x30C   | BYTE   | ISLIVE      | 1 = vivo, 0 = morto/despawned                 | ✅ Confirmado |
| 0x320   | BYTE   | TYPE        | Tipo (1=player, 2=monster, 4=NPC)            | ✅ Confirmado |
| 0x404   | float  | OFF_X       | Posição X (centésimos de tile)                 | ✅ Confirmado |
| 0x408   | float  | OFF_Y       | Posição Y (centésimos de tile)                | ✅ Confirmado |
| 0x40C   | float  | OFF_Z       | Posição Z                                     | ✅ Confirmado |

### Ponteiros do Object Manager

| Símbolo              | Endereço     | Descrição                          |
|----------------------|--------------|-------------------------------------|
| main.exe + 0xA7C224 | ObjectManager| Ponteiro raiz do Object Manager     |
| [ObjectManager + 0x8]| ObjectArray | Base do array de objetos (0x598 cada) |

---

## Problemas Encontrados e Status

### ❌ PROBLEMA 1: Direction Byte (direction byte = 0xC0)

**Sintoma**: Pacotes enviados com `dir=0xC0` (norte) em vez de `0xC5` (SW) ou outra direção correta.

**Causa Raiz**: `ComputeDirection()` usava `entityIndex & 0x3FF` para mapear entity index → slot, mas o composite index **não codifica o slot** diretamente. Exemplo:
- Slot 0 (Guard): idx=0x0292 → `0x0292 & 0x3FF = 0x92 = 146` → slot inexistente/vazio
- Slot 3 (Worm): idx=0x02AC → `0x02AC & 0x3FF = 0x2AC = 684` → fora do range (0-399)

**Correção Aplicada**: Adicionada função `FindObjectByEntityIndex()` que varre o array de objetos buscando o slot com o `OFF_INDEX` correto.

**Status**: ✅ Corrigido no código, aguardando teste em runtime.

### ❌ PROBLEMA 2: Action Byte (era 0x0A, correto é 0x04)

**Sintoma**: Pacotes enviados com `action=0x0A` em vez de `0x04`.

**Causa Raiz**: Valor hardcoded errado no código original.

**Correção**: Alterado de `0x0A` para `0x04` em `BuildAttackPacket()`.

**Status**: ✅ Corrigido e confirmado.

### ⚠️ PROBLEMA 3: Fórmula de Direção — Validação Pendente

A fórmula implementada usa `atan2(dy, dx)` com rotação de `+π/2` para alinhar com MU Online (onde 0=Norte). Precisa de validação em runtime comparando com pacotes reais do cliente oficial.

Possíveis problemas:
1. O eixo Y pode crescer para cima (não para baixo) no MU Online — se sim, a fórmula deve usar `-dy`
2. A rotação pode não ser exatamente `+π/2` — pode ser `−π/2` ou `0`
3. O mapeamento pode ser 16 direções (0-15) em vez de 8 (0-7)

### ⚠️ PROBLEMA 4: Offsets 0xFC/0x100 vs 0x404/0x408

O Main.exe usa offsets 0xFC e 0x100 para coordenadas do player na função de ataque, mas a DLL usa 0x404 e 0x408. Isso pode funcionar se ambos apontam para o mesmo dado, mas precisa verificação:
- `0xFC`/`0x100`: usados por `[ebp-0x24]` (ponteiro passado como parâmetro para a função) — pode ser uma sub-estrutura
- `0x404`/`0x408`: confirmados como coordenadas X/Y do objeto no dump

### ⚠️ PROBLEMA 5: Offsets 0xAC e 0xB0 do Target

A função de ataque lê dois bytes do target:
- `[target+0xAC]` — valor desconhecido (no dump do Guard: `0xD1`)
- `[target+0xB0]` — valor desconhecido (no dump do Guard: `0x2C`)

Esses campos são empurrados na pilha antes da chamada de construção do pacote, mas pode ser que sejam para funções internas do cliente (ex: animação), não para o pacote final de 7 bytes.

---

## Action Byte — Análise

O byte de ação no pacote de ataque é `0x04` (confirmado via captura de pacotes reais). O valor `0x0A` era usado incorretamente e causava rejeição pelo servidor.

Possíveis valores do action byte (especulação, não confirmados):
- 0x04 = ataque normal
- 0x0A = pode ser outra ação (ex: ataque com movimento, ou action cancel)

---

## Entidade vs Slot Index

| Slot | Entity Index | Attack Target ID (XOR 0xEFF5) | Nome                |
|------|-------------|-------------------------------|---------------------|
| 0    | 0x0292      | 0xED67                        | Guard               |
| 1    | 0x0297      | 0xED62                        | Shadow Phantom Soldier|
| 2    | 0x02AA      | 0xED5F                        | Worm                |
| 3    | 0x02AC      | 0xED59                        | Worm                |
| 4    | 0x0291      | 0xED64                        | Guard               |
| 94   | 0x330F      | 0xDCFA                        | EternalWiz (player) |

**Importante**: O slot index (0, 1, 2, 3...) NÃO pode ser derivado do entity index via `& 0x3FF`. É necessário varrer o array.

---

## Código Atual (ComputeDirection)

```cpp
// Busca objeto pelo entity index (varredura linear)
static uintptr_t FindObjectByEntityIndex(uint16_t entityIndex) {
    uintptr_t arrayBase = GameMemory::GetObjectArray();
    if (!arrayBase) return 0;
    for (int i = 0; i < GameAddresses::MAX_OBJECTS; ++i) {
        uintptr_t obj = arrayBase + i * GameAddresses::OBJECT_SIZE;
        uint16_t idx = *(uint16_t*)(obj + GameObject::OFF_INDEX);
        if (idx == entityIndex)
            return obj;
    }
    return 0;
}

static uint8_t ComputeDirection(uint16_t targetEntityIndex) {
    uintptr_t localPlayer = GameMemory::GetLocalPlayer();
    if (!localPlayer) return 0xC0;

    uintptr_t targetObj = FindObjectByEntityIndex(targetEntityIndex);
    if (!targetObj) return 0xC0;

    float playerX = *(float*)(localPlayer + GameObject::OFF_X);
    float playerY = *(float*)(localPlayer + GameObject::OFF_Y);
    float targetX = *(float*)(targetObj + GameObject::OFF_X);
    float targetY = *(float*)(targetObj + GameObject::OFF_Y);

    if (playerX == 0.0f && playerY == 0.0f) return 0xC0;
    if (targetX == 0.0f && targetY == 0.0f) return 0xC0;

    float dx = targetX - playerX;
    float dy = targetY - playerY;
    if (dx == 0.0f && dy == 0.0f) return 0xC0;

    const float PI = 3.14159265f;
    float angle = atan2f(dy, dx);
    float dir = floorf((angle + PI * 0.5f) / (2.0f * PI) * 8.0f);
    int dirInt = (int)dir % 8;
    if (dirInt < 0) dirInt += 8;

    return (uint8_t)(0xC0 | dirInt);
}
```

---

## TODO — Próximos Passos

1. [ ] **Validar direção em runtime**: Reiniciar o jogo, atacar um mob e verificar nos logs se `ComputeDir` retorna valores como `0xC1`-`0xC7` (não apenas `0xC0`)
2. [ ] **Comparar com pacote real do cliente**: Capturar pacote de ataque do cliente oficial e verificar se o direction byte bate
3. [ ] **Verificar se Y cresce para cima ou para baixo**: Se os valores de Y no dump (ex: 21250, 8050) mostram que Y cresce para baixo, a fórmula está correta. Se Y cresce para cima, inverter o sinal de dy
4. [ ] **Verificar se a direção é 8-dir ou 16-dir**: O byte no pacote pode ser `0xC0 | (dir & 0x0F)` com 16 direções em vez de `0xC0 | (dir & 0x07)`
5. [ ] **Investigar offsets 0xAC e 0xB0**: Determinar o que esses campos representam no target e se são relevantes para o pacote
6. [ ] **Performance**: `FindObjectByEntityIndex()` faz varredura linear em 400 slots. Considerar cache ou lookup table se necessário