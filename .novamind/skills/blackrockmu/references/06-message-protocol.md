# 6. Message protocol (44412)

Format `TAG|payload`, registered with `[MessageTag("TAG")]`, each class has a static
`Parse(string payload)`.

## byp.dll → MuProxy2
`ENTITY`, `ENTITY_BATCH_END`, `INJECT_PACKET`, `LOG`, `PKT`

## MuProxy2 → byp.dll
`ATTACK_START`, `ATTACK_STOP`, `ATTACK_CONFIG`, `ATTACK_PLAYERIDX`, `ATTACK_DUMP`,
`MONSTER_START`, `MONSTER_STOP`, `SEND_PACKET`

## To add a new message
1. Create a class with `[MessageTag("NOVO_TAG")]` in `MuProxy2/Protocol/Messages.cs`.
2. Implement static `Parse(string payload)`.
3. Add a `case` in `DllCommandService.ProcessLine()` if it's an inbound message.
4. **Never** use space as a separator — always pipe `|`.
5. The channel is bidirectional: the **byp.dll (C++) side must also parse and act on the tag**
   (see `MessageRegistry.cpp` on the C++ side). A new outbound command (MuProxy2→byp.dll) does
   nothing until the C++ handler exists.
