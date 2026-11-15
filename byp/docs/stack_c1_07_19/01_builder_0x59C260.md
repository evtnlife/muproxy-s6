# Builder do C1 07 19 — main.exe `0x59C260` (RVA 0x19C260)

Função completa: recebe skill ID + objetos, valida, monta o buffer `C1 07 19 [4 bytes]`
e chama o send. Return address no stack: `0x59C7A7` (após `call 0x404D70` em `0x59C7A2`).

## Prologue + origem do skill ID
```asm
0x59C260: push ebp
0x59C261: mov  ebp, esp
0x59C263: push -1                 ; SEH
0x59C265: push 0xCD31ED
0x59C26A: mov  eax, fs:[0]
0x59C270: push eax
0x59C271: mov  fs:[0], esp
0x59C278: mov  eax, 0x1054        ; frame size (cookie check)
0x59C27D: call 0x009CFB50         ; __security_cookie

; --- skill ID ---
0x59C282: movzx eax, byte [0x081C0398]   ; flag tabela ativa?
0x59C289: test eax, eax
0x59C28B: jz   0x59C2A6            ; flag=0 → usa índice bruto
0x59C28D: mov  ecx, [0x081C039C]   ; índice
0x59C293: mov  edx, [0x08128AC8]   ; base da tabela
0x59C299: movzx eax, word [edx+ecx*2+0x78]  ; skill wire ID (16 bits)
0x59C29E: mov  [ebp-0x1058], eax
0x59C2A4: jmp  0x59C2B2
0x59C2A6: mov  ecx, [0x081C039C]   ; fallback: índice bruto
0x59C2AC: mov  [ebp-0x1058], ecx
0x59C2B2: mov  edx, [ebp-0x1058]
0x59C2B8: mov  [ebp-0x1C], edx     ; ← skill ID local (usado no pacote)
```

## Validações
```asm
0x59C2C7: mov  eax, [ebp+8]        ; parâmetro 1 (objeto jogador?)
0x59C2CA: mov  byte [eax+0x78], 0  ; limpa flag no objeto
0x59C2CE: mov  ecx, [ebp+0xC]      ; parâmetro 2
0x59C2D1: cmp  dword [ecx+0x30], 0x48B   ; tipo/check
0x59C2D8: jnz  0x59C502            ; sai se não for 0x48B
0x59C2DE: call 0x005509C0
0x59C2E3: mov  edx, [ebp-0x1C]
0x59C2E6: mov  [ebp-0x105C], edx
```

## Dispatch por skill ID (tabela de jump)
```asm
; skill < 0x3C → sub 0x2B, jump table em 0x59CB18
0x59C2EC: cmp  [ebp-0x105C], 0x3D
0x59C2F3: jnle 0x59C32B
0x59C2F5: cmp  [ebp-0x105C], 0x3C
0x59C2FC: jnl  0x59C454            ; 0x3C/0x3D → caminho comum
0x59C302: mov  eax, [ebp-0x105C]
0x59C308: sub  eax, 0x2B
0x59C30B: mov  [ebp-0x105C], eax
0x59C311: cmp  [ebp-0x105C], 0x06
0x59C318: jnbe 0x59C4B9
0x59C31E: mov  ecx, [ebp-0x105C]
0x59C324: jmp  dword [ecx*4+0x59CB18]  ; jump table 1

; skill > 0x146 → sub 0x147, jump table em 0x59CB34
0x59C32B: cmp  [ebp-0x105C], 0x146
0x59C335: jnle 0x59C376
; ... casos 0x146, 0x4A, 0x4E, 0xEE → caminhos específicos
0x59C376: mov  edx, [ebp-0x105C]
0x59C37C: sub  edx, 0x147
0x59C382: mov  [ebp-0x105C], edx
0x59C388: cmp  [ebp-0x105C], 0xC4
0x59C392: jnbe 0x59C4B9
0x59C398: mov  eax, [ebp-0x105C]
0x59C39E: movzx ecx, byte [eax+0x59CB58]
0x59C3A5: jmp  dword [ecx*4+0x59CB34]  ; jump table 2
```

## Montagem do pacote (caminho do Force 0x3C)
```asm
; --- rate limit: 300ms entre sends ---
0x59C6E8: call dword [0x00D221B0]   ; GetTickCount()
0x59C6EE: sub  eax, [0x08793708]    ; agora - último send
0x59C6F4: push eax
0x59C6F5: call 0x009D670A           ; abs()
0x59C6FD: cmp  eax, 0x12C           ; 300ms
0x59C702: jle  0x59C7B9             ; se <= 300ms, pula (não envia)

0x59C708: call dword [0x00D221B0]   ; GetTickCount()
0x59C70E: mov  [0x08793708], eax    ; atualiza último send

; --- reset buffer ---
0x59C713: lea  ecx, [ebp-0x82C]     ; buffer local (0x82C bytes)
0x59C719: call 0x00404B40           ; Packet::Reset()

0x59C71E: mov  dword [ebp-4], 0

; --- header C1 07 19 ---
0x59C725: mov  cx, word [ebp-0x1C]  ; skill ID
0x59C729: mov  word [ebp-0x830], cx ; guarda skill em temp
0x59C730: push 0x19                 ; sub-type
0x59C732: push 0xC1                 ; type
0x59C737: lea  ecx, [ebp-0x82C]
0x59C73D: call 0x00404B90           ; Packet::SetHeader(C1, 19)

; --- 4 bytes payload: skill(2) + target(2) ---
0x59C742: mov  edx, [ebp-0x20]      ; objeto alvo
0x59C745: movsx eax, word [edx+0x7E] ; target field (16 bits)
0x59C749: and  eax, 0xFF            ; target_low
0x59C74E: push eax
0x59C74F: mov  ecx, [ebp-0x20]
0x59C752: movsx edx, word [ecx+0x7E]
0x59C756: sar  edx, 8               ; target_high
0x59C759: push edx
0x59C75A: movzx eax, word [ebp-0x830] ; skill
0x59C761: and  eax, 0xFF            ; skill_low
0x59C766: push eax
0x59C767: movzx ecx, word [ebp-0x830]
0x59C76E: shr  ecx, 8               ; skill_high
0x59C771: and  ecx, 0xFF
0x59C777: push ecx

; 4× Packet::AddByte (thiscall, ecx=buffer, param=byte)
0x59C778: lea  ecx, [ebp-0x82C]
0x59C77E: call 0x004050D0           ; AddByte(skill_high)
0x59C783: mov  ecx, eax
0x59C785: call 0x004050D0           ; AddByte(skill_low)
0x59C78A: mov  ecx, eax
0x59C78C: call 0x004050D0           ; AddByte(target_high)
0x59C791: mov  ecx, eax
0x59C793: call 0x004050D0           ; AddByte(target_low)

; --- send ---
0x59C798: push 0                    ; flag2
0x59C79A: push 1                    ; flag1 (encrypted=1)
0x59C79C: lea  ecx, [ebp-0x82C]
0x59C7A2: call 0x00404D70           ; Packet::Send(1, 0) → 0x404DB0
0x59C7A7: mov  dword [ebp-4], -1   ; ← return address no stack
```

## Layout do buffer após o builder
```
[0] = C1  (type)
[1] = 07  (length, auto)
[2] = 19  (sub-type)
[3] = skill_high   ← SERÁ SOBRESCRITO PELO SERIAL em 0x404DB0!
[4] = skill_low
[5] = target_high
[6] = target_low
```

## Observações
- O skill ID vem de `[0x081C039C]` (índice) ou de tabela `[0x08128AC8][idx*2+0x78]`.
- Com flag=0 (atual), usa índice bruto = 0x3C.
- O target vem de `[objeto_alvo+0x7E]` (16 bits).
- Rate limit: 300ms entre sends (comparação com GetTickCount).
- O buffer é local na stack (`ebp-0x82C`), tamanho 0x82C bytes.
