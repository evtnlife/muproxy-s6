# Test case: debug-corrupted-attack-packets

Realistic prompt a user would actually type:

> O ataque automático tá mandando pacote pro servidor mas ele tá recusando, parece que
> tá corrompido. Ontem tava funcionando e eu só mexi no handler de SEND_PACKET. O que
> pode ter acontecido?

## What a correct answer needs

- Point at the **double-encryption** failure mode: `InjectC1Attack` already applies
  XorData + Layer-1. If `HandleSendPacket` now feeds it XorData-form bytes, the data
  gets XorData'd twice → server rejects.
- Reference the two injection paths and that they must NOT share the XorData step
  (skill §4).
- Suggest verifying against the validated wire example in §4
  (`C1 07 11 00 1A 78 04` → XorData → `C1 07 11 EF ED B6 77` → Layer-1 → wire).
- Should NOT suggest adding a new decrypt in C#, or touching the proxy.
