# Criptografia do 55902 (BlackRock MU Online)

## Endereços (base main.dll = 0x71220000)
- Hook de recv IAT: `0x712DB1F0` (offset `0xBB1F0`)
- Hook de send IAT: `0x712DB300` (offset `0xBB300`)
- Gerador de chaves: `0x712DB620` (offset `0xBB620`)
- Instalação dos hooks IAT: `0x712DB718` - `0x712DB77C`
- XOR key storage: `0x71B23F76` (offset `0x403F76`)
- SUB key storage: `0x71B23F75` (offset `0x403F75`)

## Algoritmo
- Decrypt (recv): `plaintext = (ciphertext ^ xor_key) - sub_key`
- Encrypt (send): `ciphertext = (plaintext + sub_key) ^ xor_key`
- Aplica-se a **todos os bytes** do pacote, do início ao fim.

## Chaves atuais
- `xor_key = 0x36`
- `sub_key = 0x07`

## Geração de chaves
A partir de um word em `[ebp-0x08]`:
- `xor_key = (word & 0xFF) - 0x12`
- `sub_key = (word >> 8) - 1`

Observação: as chaves são dinâmicas, mas podem ser lidas de `0x71B23F75`/`0x71B23F76` em runtime.

## Equivalência com MuEmu
A mesma família de algoritmo aparece no `Main_EX603` do MuEmu (`Main\HackCheck.cpp`):
```cpp
void DecryptData(BYTE* lpMsg,int size) { lpMsg[n] = (lpMsg[n]^EncDecKey1)-EncDecKey2; }
void EncryptData(BYTE* lpMsg,int size) { lpMsg[n] = (lpMsg[n]+EncDecKey2)^EncDecKey1; }
```
A diferença é a geração de chaves: no BlackRock as chaves vêm de `Main.dll+0x403F75/76`, enquanto no MuEmu elas derivam de `CustomerName`/`ClientSerial` somadas a bases `0xE2`/`0x76`.

---

# Criptografia de Skill Attack (Power Ball / 0xE5)

**Descoberto em 2026-08-09 via x64dbg stack trace no send wrapper `0x405110`.**

## Stack Trace (do topo → base)

| # | Return Address | Função | Descrição |
|---|---------------|--------|-----------|
| 1 | `0x404EF9` | caller imediato | `call 0x405110` em `0x404EF4` |
| 2 | `0x404D9A` | **encrypt + send** | `0x404D70` → `0x404DB0` (encrypt) → `0x404DD1` (send) |
| 3 | `0x59CEE6` | **skill packet builder** | `call 0x404D70` em `0x59CEE1` |
| 4 | `0x5B14A7` | game loop | ... |
| 5 | `0x5B5644` | game loop | ... |
| 6 | `0x5B7745` | game loop | ... |
| 7 | `0x4D957C` | main loop | ... |
| 8 | `0x7123AD16` | **Main.dll** | ... |

## Funções-chave

### Send Wrapper: `main.exe+0x405110`
- Protótipo: `void SendWrapper(BYTE* buffer, int size)`
- Chama `0x9CF1C0` para encriptação, depois `send()` via `[0xD227F8]`
- 3 callers conhecidos: `0x404DD1`, `0x404EF4`, `0x404F88`

### Encrypt + Send de Skill: `main.exe+0x404D70`
- Recebe struct via `ecx`:
  - `[ecx]` = word (tamanho do payload?)
  - `[ecx+2]` = buffer payload (plaintext)
- Chama `0x404FA0` (init), depois `0x404DB0` (encrypt), depois `0x404DD1` (send wrapper)

### Função de Encriptação: `main.exe+0x404DB0`
- **Chave fixa de 6 bytes**: `CD 41 26 3C 4E 4D`
- Algoritmo: XOR encadeado com a chave (diferente do XOR+sub do resto do jogo!)
- A chave é setada em `0x404D01`:
  ```asm
  mov byte [ebp-0x06], 0xCD
  mov byte [ebp-0x05], 0x41
  mov byte [ebp-0x04], 0x26
  mov byte [ebp-0x03], 0x3C
  mov byte [ebp-0x02], 0x4E
  mov byte [ebp-0x01], 0x4D
  ```
- Lógica (pseudocode):
  ```cpp
  for (i = 0; i < size; i++) {
      key_idx = i & 0x1F;  // mod 32
      // XOR com chave e bytes vizinhos do buffer
      buffer[i] ^= key[key_idx];
      // ... XOR encadeado com bytes adjacentes
  }
  ```

### Skill Packet Builder: `main.exe+0x59CEE1`
- Constrói o struct com plaintext em `[ebp-0x81C]`
- Chama `0x404D70` para encriptar e enviar
- **Breakpoint em `0x59CEE1`** → captura plaintext antes da encriptação

## Pacote Skill Attack (formato observado)

### Encriptado (13 bytes):
```
C3 0D 41 2B 45 04 49 41 55 A8 B5 3F 04
```
- `C3` = header (encrypted)
- `0D` = size (13 bytes)
- `41 2B 45 04 49 41 55 A8 B5 3F 04` = payload encriptado

### Plaintext (a ser capturado no breakpoint `0x59CEE1`):
- Tamanho total: 13 bytes
- Header: `C3 0D`
- Opcode: `E5` (SKILL_ATTACK)
- 10 bytes de payload (skill ID, coordenadas, direção, ângulo, target index, magic key)

## Implicações para o byp.dll

1. **NÃO podemos usar a criptografia normal** (XOR 0x36, sub 0x07) para skill attack
2. Precisamos implementar o algoritmo de XOR encadeado com chave `CD 41 26 3C 4E 4D`
3. OU melhor: enviar o pacote **já encriptado** (capturar plaintext → encriptar com a chave → enviar)
4. O formato do plaintext ainda precisa ser confirmado (capturar no breakpoint `0x59CEE1`)
