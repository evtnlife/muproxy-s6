# Thunk/Caller — main.exe `0x59B3C0` → Main.dll

O caller do builder (return `0x59BA17`) chama `0x59B3C0` em `0x59BA12`.
`0x59B3C0` é um **thunk** que faz `JMP` para Main.dll.

## 0x59B3C0 — Thunk
```asm
0x59B3C0: jmp 0x67D68330    ; → Main.dll+0x148330
```

## 0x59BA12 — Call site no caller
```asm
; (função grande, usa ebp-0x838, ebp+0x0C)
0x59BA12: call 0x0059B3C0   ; → thunk → Main.dll
0x59BA17: add  esp, 0x10   ; ← return address no stack
0x59BA1A: jmp  0x0059C24E   ; continua em 0x59C24E (antes do builder)
```

## Main.dll+0x1484F1 (0x67D684F1)
Esta é a função em Main.dll que é chamada pelo thunk e que por sua vez
chama o builder (`0x59C260`). O return address `0x67D684F1` no stack
indica que o call para o builder está em `0x67D684F0` (ou nearby).

## Fluxo completo
```
Caller (main.exe, contém 0x59BA12)
  → call 0x59B3C0 (thunk)
    → jmp 0x67D68330 (Main.dll)
      → [Main.dll function]
        → call 0x59C260 (builder)
          → [builder monta C1 07 19 + 4 bytes]
          → call 0x404D70 (send wrapper)
            → call 0x404DB0 (send + serial + SM encrypt)
              → call 0x405110 (Net::Send)
                → [Main.dll send final → ws2_32]
```

## Observações
- O thunk em `0x59B3C0` é um simples `JMP` — não há lógica nele.
- A função real está em Main.dll (`0x67D68330`).
- O caller em main.exe continua em `0x59C24E` após o call, que é
  14 bytes antes do builder (`0x59C260`). Isso sugere que o caller
  e o builder estão na mesma função grande, ou que há um tail call.
