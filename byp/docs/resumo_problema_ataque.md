# Resumo completo do problema de ataque — BlackRockMU / byp.dll / MuProxy2

## 1. Arquitetura

- **Cliente:** `main.exe` (MU Online x86) carrega `byp.dll` (gg_bypass2) via proxy OpenGL.
- **Proxy TCP:** `MuProxy2.exe` (C# .NET 8 WinForms).
  - Porta **44412**: CMD / OBJ / INJECT / ATTACK (TcpClient da DLL).
  - Porta **44413**: PKT / SEND capture (PacketSniffer da DLL).
- **Game Server:** `54.36.205.234:55902` (BlackRockMU).
- **Objetivo:** atacar monstros automaticamente sem tomar DC.

## 2. Protocolo de ataque 0x11

Pacote de ataque normal do cliente:

```
C1 07 11 <targetHi> <targetLo> <action> <dir>
```

- `0x11` = opcode ATTACK
- `action` = `0x0A` (ataque normal) ou similar
- `dir` = direção do personagem

## 3. O que funcionava antes (git HEAD~1)

No commit anterior, `MuProxy2/Protocol/MonsterAttacker.cs` enviava o **entity index direto** para a DLL:

```csharp
pkt[3] = (byte)(index & 0xFF);      // lo byte primeiro
pkt[4] = (byte)((index >> 8) & 0xFF); // hi byte depois
```

Ou seja, o pacote `0x11` saía em **little-endian** com o índice do monstro.

## 4. O que foi quebrado

No commit atual, alguém introduziu um `AttackIndex` "composto":

```csharp
public static ushort ComputeAttackIndex(byte idxLo, byte classLo)
    => (ushort)((idxLo << 8) | classLo);
```

Isso gerou targets errados (ex: `A318` em vez de `02A3`) e causou DC imediato.

## 5. Correções já aplicadas

### 5.1 C# — MuProxy2/Protocol/MonsterAttacker.cs

- `AttackTarget()` e `StartContinuousAttack()` agora usam `entityInfo.RawIndex` (fallback para `entityIndex`).
- Não usam mais `AttackIndex` composto.
- Adicionada proteção para `targetIndex == 0`.

### 5.2 C# — MuProxy2/MainForm.cs

- `MonsterAttacker.Enabled = false` no startup (antes estava `true`, causando ataque automático indesejado).
- Apenas o checkbox "Enable Atack" (`checkBox1`) ativa o ataque.
- Captura de mobs (`EnableEntityDiscovery`) continua independente.

## 6. Problema que ainda persiste

### 6.1 Logs reais de ataque pela UI

Quando o usuário ataca um monstro pela UI do jogo, o cliente envia pacotes como:

```
SEND  21:23:01.071  7  AttackMonster  C1-07-11-ED-57-0C-CD
SEND  21:23:04.181  7  AttackMonster  C1-07-11-ED-51-0A-C9
SEND  21:23:07.058  7  AttackMonster  C1-07-11-ED-40-1B-DC
```

### 6.2 Monster List na UI do proxy

A UI mostra:

```
02B5  Monster 02B5  Entity
02A4  Monster 02A4  Entity
02A3  Monster 02A3  Entity
02A2  Class 24      Type24
0298  Class 257     Type257
```

### 6.3 Discrepância

Os targets nos pacotes reais começam com `ED` (ex: `ED-57`), enquanto os índices mostrados na UI começam com `02` (ex: `02B5`).

Isso prova que o pacote `0x11` **não usa o entity index do servidor diretamente**. Ele usa um **índice composto derivado do ObjectManager do cliente**.

## 7. Hipótese atual

O pacote `0x11` provavelmente usa:

```
target = (ObjectManagerSlot << 8) | MonsterClassLo
```

Ou similar, onde:

- `ObjectManagerSlot` = posição do monstro no array interno de objetos do `main.exe` (0..399, tamanho de slot `0x598`).
- `MonsterClassLo` = byte baixo da classe do monstro.

Exemplo: se o slot for `0xED` e a classe for `0x57`, o pacote sai `11 ED 57 ...`.

## 8. Estrutura do ObjectManager (já mapeada)

```cpp
// GameAddresses.h
static constexpr uintptr_t OBJ_MANAGER_PTR  = 0xA7C224;
static constexpr uintptr_t OBJ_ARRAY_OFFSET = 0x8;
static constexpr int MAX_OBJECTS  = 400;
static constexpr int OBJECT_SIZE  = 0x598;

// GameObject.h
struct GameObject {
    static constexpr int OFF_NAME     = 0x038;
    static constexpr int OFF_INDEX    = 0x07E;  // word composite index
    static constexpr int OFF_LEVEL    = 0x19C;
    static constexpr int OFF_LEVEL2   = 0x208;
    static constexpr int OFF_INTPTR   = 0x29C;
    static constexpr int OFF_ISLIVE   = 0x30C;
    static constexpr int OFF_TYPE     = 0x320;  // byte (ObjType enum)
    static constexpr int OFF_FLOAT1   = 0x338;
    static constexpr int OFF_SET      = 0x368;
    static constexpr int OFF_X        = 0x404;
    static constexpr int OFF_Y        = 0x408;
    static constexpr int OFF_Z        = 0x40C;
    static constexpr int OFF_X_SET    = 0x410;
    static constexpr int OFF_Y_SET    = 0x414;
    static constexpr int OFF_Z_SET    = 0x418;
};
```

Acesso:

```cpp
manager = *[main.exe + 0xA7C224]
array   = *[manager + 0x8]
object  = array + i * 0x598
```

## 9. O que falta descobrir

1. **Qual campo do objeto contém o "entity index do servidor"?**
   - `OFF_INDEX = 0x07E` é descrito como "word composite index".
   - Pode ser o mesmo índice `02B5` da UI, ou pode ser o índice composto usado no pacote `0x11`.

2. **Qual campo contém a monster class?**
   - `OFF_TYPE = 0x320` é um byte `ObjType enum`.
   - Mas a classe real do monstro (ex: 0x57, 0x51, 0x40) pode estar em outro offset (WORD).

3. **Como montar o target correto para o pacote 0x11?**
   - Opção A: `(ObjectManagerSlot << 8) | ClassLo`
   - Opção B: `(ClassLo << 8) | ObjectManagerSlot`
   - Opção C: alguma combinação com `OFF_INDEX` e class.

4. **O offset 0x07E realmente guarda o índice composto do pacote 0x11?**
   - Se sim, basta ler `*(uint16_t*)(obj + 0x07E)` e usar como target.

## 10. Servidor tem anti-hack

O servidor usa `CHackPacketCheck` (arquivo `HackPacketCheck.cpp`):

```cpp
bool CheckPacketHack(int aIndex, int index, int value, int encrypt, int serial)
```

Validações:

- Opcode/índice de pacote desconhecido → DC.
- Criptografia errada → DC.
- Serial fora de sincronia → DC.
- Rate limit (`MaxDelay`, `MinCount`, `MaxCount`) → DC.

Isso significa que **injetar pacotes diretamente no socket do game server é arriscado**. A injeção direta pode falhar no serial check porque não atualiza o contador de serial interno do cliente.

A solução mais robusta seria chamar a função do próprio cliente (`main.exe`) que envia o pacote de ataque, mas o endereço dessa função ainda não foi descoberto.

## 11. Objetivo para o próximo modelo

- Determinar o formato exato do target no pacote `0x11`.
- Mapear no ObjectManager do `main.exe`:
  - Onde fica o entity index do servidor (`02B5`).
  - Onde fica o índice composto usado no pacote de ataque.
  - Onde fica a monster class.
- Ajustar `AttackModule.cpp` para buscar o objeto pelo entity index do servidor e montar o pacote `0x11` com o índice correto.
- Ou, alternativamente, encontrar o endereço da função `CGAttack`/`SendAttack` do cliente e chamá-la via hook/remoto call.

## 12. Arquivos relevantes

- `MuProxy2/MuProxy2/Protocol/MonsterAttacker.cs`
- `MuProxy2/MuProxy2/Protocol/MonsterTracker.cs`
- `MuProxy2/MuProxy2/MainForm.cs`
- `gg_bypass2/src/AttackModule.cpp`
- `gg_bypass2/src/GameMemory.cpp`
- `gg_bypass2/src/include/GameObject.h`
- `gg_bypass2/src/include/GameAddresses.h`
- `gg_bypass2/src/include/ObjType.h`
- `docs/PS_Object_Array.md`

## 13. Dados do teste mais recente

Pacotes reais de ataque pela UI:

```
C1-07-11-ED-57-0C-CD
C1-07-11-ED-51-0A-C9
C1-07-11-ED-40-1B-DC
C1-07-11-ED-56-0D-CC
```

Monster List:

```
02B5  Monster 02B5  Entity
02A4  Monster 02A4  Entity
02A3  Monster 02A3  Entity
02A2  Class 24      Type24
0298  Class 257     Type257
```

Ação/dir variam. O byte `ED` é constante nesse exemplo, sugerindo que pode ser o slot do ObjectManager do monstro atualmente selecionado.
