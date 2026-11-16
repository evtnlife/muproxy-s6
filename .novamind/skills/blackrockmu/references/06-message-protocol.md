# 6. Message protocol (44412)

Format `TAG|payload`, registered with `[MessageTag("TAG")]`, each class has a static
`Parse(string payload)`.

## byp.dll → MuProxy2
`ENTITY`, `ENTITY_BATCH_END`, `LOG`, `PKT`

> `INJECT_PACKET` was **removed** from the protocol — do not look for it.

## MuProxy2 → byp.dll (tags registered in the C++ `MessageRegistry`)
| Tag | Registered in | Payload / notes |
|-----|---------------|-----------------|
| `ATTACK_START` | `attack/AttackModule.cpp` | `delay=…|range=…|type=…|hitcount=…` |
| `ATTACK_STOP` | `attack/AttackModule.cpp` | — |
| `ATTACK_CONFIG` | `attack/AttackModule.cpp` | `type=…|hitcount=…` |
| `ATTACK_PLAYERIDX` | `attack/AttackModule.cpp` | `index=…` (composite index do player local) |
| `SEND_PACKET` | `attack/AttackModule.cpp` | `dir|hex` → `PacketInjector::InjectXorData` |
| `MONSTER_START` / `MONSTER_STOP` | `attack/AttackModule.cpp` | tracker de monstros |
| `ATTACK_DUMP` | `debug/DebugTools.cpp` | despeja o ObjectManager completo |
| `ATTACK_HEXDUMP` | `debug/DebugTools.cpp` | `slot=N` |
| `ATTACK_SCANVALUE` | `debug/DebugScans.cpp` | `value=X` (hex) — busca o WORD em todos os slots |
| `ATTACK_SCANSLOT` | `debug/DebugScans.cpp` | `slot=N|value=X` |
| `ATTACK_DIRSCAN` | `debug/DebugScans.cpp` | byte de direção no player local |
| `CMD` | `net/PacketHandler.cpp` | legado `CMD|OPCODE|SUBCMD|data` (usado pelo MonsterTracker) |

- The five `ATTACK_*` debug tags exist **only on the C++ side** — there is no `[MessageTag]`
  class for them in `Messages.cs`, so the MuProxy2 UI cannot send them yet. Send the raw line
  (`ATTACK_HEXDUMP|slot=3`) to trigger them.
- Payloads are `key=value` pairs separated by `|`. Parse them with `CommandParams`
  (`include/CommandParams.h`): `Find`, `ParseIntParam`, `ParseByteParam`, `ParseFloatParam`,
  `ParseHexParam`.

## To add a new message
1. Create a class with `[MessageTag("NOVO_TAG")]` in `MuProxy2/Protocol/Messages.cs`.
2. Implement static `Parse(string payload)`.
3. Add a `case` in `DllCommandService.ProcessLine()` if it's an inbound message.
4. **Never** use space as a separator — always pipe `|`.
5. The channel is bidirectional: the **byp.dll (C++) side must also parse and act on the tag**
   — add `REGISTER_MESSAGE(TAG, Handler)` in the module that owns the feature (the macro is in
   `include/MessageRegistry.h`). A new outbound command (MuProxy2→byp.dll) does nothing until the
   C++ handler exists.
6. If the handler lives in a **new** module, remember: `src/<domain>/<Name>.cpp` + header in
   `src/include/`, and add the `.cpp` to the `SOURCES` list in `build.bat` (§7, §12).
