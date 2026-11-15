# Métodos do Objeto Pacote — main.exe

O buffer local no builder (`[ebp-0x82C]`) é um **objeto pacote** com layout:

```
[0..1]  = WORD  length (nº de bytes de dados)
[2..]   = bytes de dados (máx 0x800)
```

## 0x404B40 — `Packet::Reset()`
```asm
0x404B40: push ebp
0x404B41: mov  ebp, esp
0x404B43: push ecx
0x404B44: mov  [ebp-4], ecx
0x404B47: mov  ecx, [ebp-4]
0x404B4A: call 0x00404B70      ; Clear()
0x404B4F: mov  eax, [ebp-4]   ; return this
0x404B52: mov  esp, ebp
0x404B54: pop  ebp
0x404B55: ret
```

## 0x404B70 — `Packet::Clear()`
```asm
0x404B70: push ebp
0x404B71: mov  ebp, esp
0x404B73: push ecx
0x404B74: mov  [ebp-4], ecx
0x404B77: xor  eax, eax
0x404B79: mov  ecx, [ebp-4]
0x404B7C: mov  word [ecx], ax   ; ← length = 0
0x404B7F: mov  esp, ebp
0x404B81: pop  ebp
0x404B82: ret
```

## 0x404B90 — `Packet::SetHeader(type, subtype)`
Parâmetros: `ecx=this`, `[ebp+8]=type`, `[ebp+0xC]=subtype`

```asm
0x404B90: push ebp
0x404B91: mov  ebp, esp
0x404B93: sub  esp, 8
0x404B96: mov  [ebp-4], ecx
; 1. Clear
0x404B99: mov  ecx, [ebp-4]
0x404B9C: call 0x00404B70       ; Clear()
; 2. Escreve type byte
0x404BA1: push 0
0x404BA3: push 1
0x404BA5: lea  eax, [ebp+8]     ; &type
0x404BA8: push eax
0x404BA9: mov  ecx, [ebp-4]
0x404BAC: call 0x00404C10       ; InsertAt(&type, offset=1, flag=0)
; 3. Lê type de volta
0x404BB1: mov  cl, byte [ebp+8]
0x404BB4: mov  byte [ebp-8], cl
; 4. Dispatch por type
0x404BB7: cmp  byte [ebp-8], 0xC1
0x404BBB: jz   0x404BC5         ; C1 → headerLen=1
0x404BBD: cmp  byte [ebp-8], 0xC2
0x404BC1: jz   0x404BD7         ; C2 → headerLen=2
0x404BC3: jmp  0x404BE9         ; outro → sem subtype
; C1: escreve subtype em offset 1
0x404BC5: push 0
0x404BC7: push 1
0x404BC9: mov  edx, [ebp-4]
0x404BCC: push edx
0x404BCD: mov  ecx, [ebp-4]
0x404BD0: call 0x00404C10       ; InsertAt(this, offset=1, flag=0)
0x404BD5: jmp  0x404BEB
; C2: escreve subtype em offset 2
0x404BD7: push 0
0x404BD9: push 2
0x404BDB: mov  eax, [ebp-4]
0x404BDE: push eax
0x404BDF: mov  ecx, [ebp-4]
0x404BE2: call 0x00404C10       ; InsertAt(this, offset=2, flag=0)
0x404BE7: jmp  0x404BEB
; Escreve subtype
0x404BEB: push 0
0x404BED: push 1
0x404BEF: lea  ecx, [ebp+0xC]   ; &subtype
0x404BF2: push ecx
0x404BF3: mov  ecx, [ebp-4]
0x404BF6: call 0x00404C10       ; InsertAt(&subtype, offset=?, flag=0)
```

## 0x404C10 — `Packet::InsertAt(src, offset, flag)`
Parâmetros: `ecx=this`, `[ebp+8]=src`, `[ebp+0xC]=offset`, `[ebp+0x10]=flag`

```asm
0x404C10: push ebp
0x404C11: mov  ebp, esp
0x404C13: push ecx
0x404C14: mov  [ebp-4], ecx
0x404C17: mov  eax, [ebp-4]
0x404C1A: movzx ecx, word [eax]     ; ecx = length
0x404C1D: movzx edx, word [ebp+0xC] ; edx = offset
0x404C21: add  ecx, edx
0x404C23: cmp  ecx, 0x800           ; max size check
0x404C29: jle  0x404C2D
0x404C2B: jmp  0x404C82             ; error
; memcpy(this + length + 2 + offset, src, offset)
0x404C2D: movzx eax, word [ebp+0xC] ; eax = offset (count?)
0x404C31: push eax
0x404C32: mov  ecx, [ebp+8]         ; ecx = src
0x404C35: push ecx
0x404C36: mov  edx, [ebp-4]
0x404C39: movzx eax, word [edx]     ; eax = length
0x404C3C: mov  ecx, [ebp-4]
0x404C3F: lea  edx, [ecx+eax+2]    ; dst = this + length + 2
0x404C43: push edx
0x404C44: call 0x009CF1C0          ; memcpy(dst, src, count)
0x404C49: add  esp, 0xC
; Se flag != 0, call extra para 0x404C90 (init/checksum)
0x404C4C: cmp  dword [ebp+0x10], 0
0x404C50: jz   0x404C70
0x404C52: push 1
...
0x404C6B: call 0x00404C90          ; (flag-specific init)
; length update — UNCONDICIONAL (ambos os caminhos chegam aqui)
0x404C70: movzx edx, word [ebp+0xC] ; edx = count
0x404C74: mov  eax, [ebp-4]
0x404C77: movzx ecx, word [eax]     ; ecx = current length
0x404C7A: add  ecx, edx             ; ecx = length + count
0x404C7C: mov  edx, [ebp-4]
0x404C7F: mov  word [edx], cx       ; ← word[this] = length + count
0x404C82: mov  esp, ebp
0x404C84: pop  ebp
0x404C85: ret  0x0C
```

## 0x4050D0 — `Packet::AddByte(byte)`
Parâmetros: `ecx=this`, `[ebp+8]=byte`

Adiciona 1 byte ao fim do buffer e incrementa o length.

## Fluxo completo no builder
```
1. Reset()        → length = 0
2. SetHeader(C1,19) → buffer = [C1][19], length = 2
3. AddByte(skill_high)   → buffer = [C1][19][skillH], length = 3
4. AddByte(skill_low)    → buffer = [C1][19][skillH][skillL], length = 4
5. AddByte(target_high)  → buffer = [C1][19][skillH][skillL][targetH], length = 5
6. AddByte(target_low)   → buffer = [C1][19][skillH][skillL][targetH][targetL], length = 6
7. Send(1, 0)     → passa (buffer+2, length, 1, 0) para 0x404DB0
```

**Nota:** O byte de length (`07`) faz parte do header escrito pelo `SetHeader(C1, 19)`. O header C1 tem 3 bytes: `C1 07 19`. O builder monta 3 (header) + 4 (payload) = 7 bytes de dados. O `Send` passa `(buffer+2, 7, 1, 0)` para `0x404DB0`, que copia os 7 bytes, sobrescreve `data[3]` com o serial, e SM-encrypta os 4 bytes finais.
