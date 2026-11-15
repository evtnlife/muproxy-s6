# Send + Serial + SM Encrypt — main.exe `0x404DB0` (RVA 0x4DB0)

Chamada pelo wrapper `0x404D70` com `(data=buffer+2, len=word[buffer], flag=1, flag2=0)`.
Return addresses no stack: `0x404D9A` (no wrapper) e `0x404EF9` (fim do caminho C3).

## Assinatura
```
0x404DB0(data, len, flag, flag2)
  data  = [ebp+8]    → ponteiro p/ dados (começa com C1)
  len   = [ebp+0xC]  → nº de bytes
  flag  = [ebp+0x10] → 0 = send plaintext, !=0 = SM-encrypted
  flag2 = [ebp+0x14] → !=0 força caminho C4
```

## Caminho flag==0 (plaintext)
```asm
0x404DC3: push [ebp+0xC]        ; len
0x404DC7: push [ebp+8]          ; data
0x404DCB: mov  ecx, [0x00E61F84]  ; objeto de rede global
0x404DD1: call 0x00405110       ; Net::Send(data, len)
0x404DD6: jmp  0x404F8D
```

## Caminho flag!=0 (SM-encrypted) — o nosso C3
```asm
; --- copia para buffer local ---
0x404DEA: call 0x009CF1C0       ; memcpy(local[ebp-0x2118], data, len)

; --- headerLen: C1 → 3, senão → 2 ---
0x404E0F: movzx eax, byte [local+0]
0x404E18: cmp  eax, 0xC1
0x404E1D: setnz cl
0x404E20: add  ecx, 2           ; headerLen = 3 (C1) ou 2
0x404E23: mov  [ebp-0x211C], ecx

; --- *** SERIAL: sobrescreve local[headerLen] *** ---
0x404E2F: mov  al, byte [0x08793700]    ; MAIN_PACKET_SERIAL (decKey)
0x404E34: mov  byte [local+headerLen], al
0x404E3B: mov  cl, byte [0x08793700]
0x404E41: add  cl, 1
0x404E44: mov  byte [0x08793700], cl    ; serial++

; --- SM::Encrypt #1 (dry-run, dst=0 → só calcula tamanho) ---
0x404E59: mov  eax, [ebp+0xC]
0x404E5C: sub  eax, [ebp-0x211C]        ; payloadLen = len - headerLen
0x404E62: push eax
0x404E69: lea  edx, [local+headerLen]   ; src
0x404E70: push edx
0x404E71: push 0                        ; dst = 0 (dry-run)
0x404E73: mov  ecx, 0x8797778           ; objeto SM global
0x404E78: call 0x00998420               ; → eax = encLen
0x404E7D: mov  [ebp-0x1114], eax        ; encLen1

; --- dispatch C3 (pequeno) vs C4 (grande) ---
0x404E83: cmp  [ebp-0x1114], 0x100      ; encLen > 255?
0x404E8D: jnl  0x404F03                 ; sim → C4
0x404E8F: cmp  [ebp+0x14], 0            ; flag2?
0x404E93: jnz  0x404F03                 ; flag2!=0 → C4

; === CAMINHO C3 (nossa captura) ===
0x404E95: mov  eax, [ebp-0x1114]
0x404E9B: add  eax, 2                   ; totalLen = encLen + 2
0x404E9E: mov  [ebp-0x2120], eax
0x404EA4: mov  byte [ebp-0x108], 0xC3   ; out[0] = C3
0x404EAB: mov  cl, byte [ebp-0x2120]
0x404EB1: mov  byte [ebp-0x107], cl     ; out[1] = totalLen

; --- SM::Encrypt #2 (real, dst=out+2) ---
0x404EB7: mov  edx, [ebp+0xC]
0x404EBA: sub  edx, [ebp-0x211C]        ; payloadLen
0x404EC0: push edx
0x404EC7: lea  ecx, [local+headerLen]   ; src
0x404ECE: push ecx
0x404ECF: lea  edx, [ebp-0x106]         ; dst = out+2
0x404ED5: push edx
0x404ED6: mov  ecx, 0x8797778
0x404EDB: call 0x00998420               ; SM::Encrypt(out+2, src, payloadLen)

; --- send ---
0x404EE0: mov  eax, [ebp-0x2120]        ; totalLen
0x404EE6: push eax
0x404EE7: lea  ecx, [ebp-0x108]         ; out
0x404EED: push ecx
0x404EEE: mov  ecx, [0x00E61F84]        ; objeto de rede
0x404EF4: call 0x00405110               ; Net::Send(out, totalLen)
0x404EF9: jmp  0x404F8D                 ; ← return address no stack
```

## Caminho C4 (pacotes grandes, encLen > 255 ou flag2)
```asm
0x404F03: mov  edx, [ebp-0x1114]
0x404F09: add  edx, 3                   ; totalLen = encLen + 3
0x404F12: mov  byte [ebp-0x1110], 0xC4  ; out[0] = C4
0x404F2D: mov  byte [ebp-0x110E], al    ; out[1] = totalLen & 0xFF
0x404F45: mov  byte [ebp-0x110F], al    ; out[2] = totalLen >> 8
; SM::Encrypt(out+3, local+headerLen, payloadLen)
0x404F6F: call 0x00998420
; Net::Send(out, totalLen)
0x404F88: call 0x00405110
```

## Resumo do fluxo C3 (nossa captura)
```
Plaintext (7 bytes):  C1 07 19 [skillH] [skillL] [targetH] [targetL]
                       ↓ headerLen=3
Serial write:         C1 07 19 [SERIAL] [skillL] [targetH] [targetL]
                       ↓ SM::Encrypt (4 bytes → 11 bytes)
Wire C3 (13 bytes):   C3 0D [11 bytes SM ciphertext]
```

**O byte `skill_high` é PERDIDO** — sobrescrito pelo serial. O receiver só vê:
`[serial][skill_low][target_high][target_low]` após decrypt.

## Endereços-chave
| Endereço | Significado |
|---|---|
| `0x08793700` | MAIN_PACKET_SERIAL (decKey, incrementa a cada C1) |
| `0x8797778` | Objeto SM global (SimpleModulusCipher) |
| `0x00998420` | SM::Encrypt(dest, src, size) |
| `0x00E61F84` | Objeto de rede global |
| `0x00405110` | Net::Send(data, len) |
