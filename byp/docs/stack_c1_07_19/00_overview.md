# Stack C1 07 19 (skill attack) — Mapa

Captura: `C1 07 19 E7 C3 E2 CF` (Force, alvo E2CF), enviada via C3 SM-encrypted.

## Bases (sessão x32dbg 2026-11-15)
| Módulo | Base |
|---|---|
| main.exe | `0x400000` |
| Main.dll | `0x67C20000` |

## Cadeia de chamadas (da UI até o socket)
```
[thread entry main.exe]
  → F12  main.exe+0x5DB688  (0x9DB688)   game loop / dispatch
  → F11  main.exe+0xD34A4   (0x4D34A4)
  → F10  main.exe+0xDADA9   (0x4DADA9)
  → F9   main.exe+0xD957C   (0x4D957C)
  → F8   main.exe+0x1B7745  (0x5B7745)
  → F7   main.exe+0x1B554F  (0x5B554F)
  → F6   main.exe+0x19BA17  (0x59BA17)   caller: call 0x59B3C0 em 0x59BA12
  → F5   main.exe+0x59B3C0  → JMP 0x67D68330 (thunk p/ Main.dll)
  → F4'  Main.dll+0x1484F1  (0x67D684F1) chama o builder
  → F4   BUILDER main.exe 0x59C260 (ret 0x59C7A7 após call send)
  → F3   main.exe+0x4D70    (0x404D70)   wrapper de send
  → F2   main.exe+0x4DB0    (0x404DB0)   send + serial + SM encrypt (ret 0x404D9A)
  → F1   main.exe+0x4EF9 / 0x5153        (0x404EF9 / 0x405153)
  → F0   Main.dll+0xBB3DE   (0x67C2BB3DE) send final → ws2_32
```

## Arquivos desta análise (um por etapa)
| # | Arquivo | Conteúdo |
|---|---|---|
| 01 | `01_builder_0x59C260.md` | Builder do C1 07 19 (função completa) |
| 02 | `02_packet_obj_methods.md` | Métodos do objeto pacote (0x404B40/0x404B90/0x4050D0) |
| 03 | `03_send_encrypt_0x404DB0.md` | Send + serial + SM encrypt |
| 04 | `04_thunk_0x59B3C0.md` | Thunk/caller 0x59B3C0 → Main.dll |
| 05+ | (próximas etapas) | Frames superiores e Main.dll |

## Descobertas-chave (corrigem skill_0x19_diff_analysis.md)
1. **O "skill wire 0xE7C3" era uma leitura errada.** O byte `E7` é o **serial**
   (MAIN_PACKET_SERIAL em `0x08793700`), escrito por `0x404DB0` em `data[3]`
   **antes** do SM encrypt, sobrescrevendo o `skill_high` que o builder escreveu.
   O skill no ar é só **1 byte** (`C3` para Force... a verificar — ver item 3).
2. **Plaintext real (7 bytes):** `C1 07 19 [serial] [b4] [b5] [b6]`
   onde `[b4..b6]` = `C3 E2 CF` na captura. SM encrypt cobre os 4 bytes finais
   (`data[3..6]`) → 11 bytes de ciphertext → wire `C3 0D [11 bytes]`.
3. **A verificar com breakpoint live:** qual valor exato o builder escreve em
   `data[3]`/`data[4]` (skill) e de onde vem o `E2CF` (target, campo `+0x7E`
   do objeto-alvo em `[ebp-0x20]`).
4. **Rate limit:** 300ms entre sends C1 (comparação `GetTickCount` vs
   `[0x08793708]`, `cmp 0x12C`).
5. **Caminho C4** existe para pacotes com ciphertext > 255 bytes
   (`C4 [len&FF] [len>>8] [ciphertext]`).

## Estado das variáveis globais lidas ao vivo
| Endereço | Valor | Significado |
|---|---|---|
| `0x081C0398` | `0x00` | flag "tabela de skill wire IDs ativa" |
| `0x081C039C` | `0x3C` | índice/skill ID bruto (60 = Force) |
| `0x08128AC8` | `0x0F9F0050` | base da tabela de skill wire IDs |
| `0x08793700` | (serial) | MAIN_PACKET_SERIAL (decKey do C3) |
| `0x08793708` | (dword) | GetTickCount do último send (rate limit 0x12C ms) |
