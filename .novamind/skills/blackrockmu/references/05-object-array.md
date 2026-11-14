# 5. Object array & offsets

## Access chain
```
manager_ptr = *(DWORD*)(main.exe + 0xA7C224)
array_base  = *(DWORD*)(manager_ptr + 0x8)
object[i]   = array_base + i * 0x598      (MAX_OBJECTS = 400, OBJECT_SIZE = 0x598)
```

## GameObject offsets (source of truth: `GameObject.h`, MuEmu Object.h, 1432 bytes)
| Offset | Type | Field |
|--------|------|-------|
| 0x00E  | bool | InSafeZone |
| 0x013  | byte | Class |
| 0x015  | byte | CtlCode (0x10 = GM) |
| 0x01F  | byte | GensInfluence (0=None,1=D,2=V) |
| 0x038  | char[25] | Name |
| 0x07E  | word | aIndex (composite entity index) |
| 0x084  | word | ID |
| 0x0AC  | dword | MapPosX |
| 0x0B0  | dword | MapPosY |
| 0x10C  | short | HelmSlot |
| 0x130  | short | ArmorSlot |
| 0x154  | short | PantsSlot |
| 0x178  | short | GlovesSlot |
| 0x19C  | short | BootsSlot |
| 0x1C0  | short | WeaponFirstSlot |
| 0x1E4  | short | WeaponSecondSlot |
| 0x208  | short | WingsSlot |
| 0x22C  | short | PetSlot |
| 0x308  | ObjectModel | model (648 bytes) |
| 0x30C  | byte | IsLive (1 = alive) — confirmed on BlackRock |
| 0x320  | byte | Type (ObjType: 0=empty,1=player,2=monster,4=NPC) |
| 0x404  | float | world X |
| 0x408  | float | world Y |
| 0x40C  | float | world Z |
| 0x110  | float | direction angle |

> ⚠️ The older skill listed `0x19C` as "Level" and `0x208` as "Level2" — those are actually
> `BootsSlot` / `WingsSlot`. Use the table above (`GameObject.h` is the source of truth).

## Other globals (main.exe RVAs, source of truth: `GameAddresses.h`)
| Name | RVA | Note |
|------|-----|------|
| OBJ_MANAGER_PTR | 0xA7C224 | DWORD* → Object Manager |
| OBJ_ARRAY_OFFSET | 0x8 | offset inside Object Manager |
| LOCAL_PLAYER_PTR | 0x7BC4F04 | DWORD* → local player GameObject |
| CHARACTER_PTR | 0xA96B0C | direct struct: name, class, skills (no level) |
| USER_OBJECT_PTR | 0x8128AC8 | DWORD* → user object struct (name, level, stats) |
| MAP_NUMBER | 0xA61E18 | DWORD (current map id) |
| VIEWPORT_WIDTH | 0xA61E58 | DWORD (px) |
| VIEWPORT_HEIGHT | 0xA61E5C | DWORD (px) |

## Target / skill globals (ABSOLUTE addresses, not RVAs)
| Name | Address | Note |
|------|---------|------|
| TARGET_SLOT_GLOBAL | 0x081C03A0 | selected target slot (-1 if none) |
| TARGET_ENTITY_INDEX_GLOBAL | 0x081C03A4 | target entity index / aIndex (used in skill packets) |

## Skill builder + cipher functions (main.exe RVAs, base `0x00400000`)
| Name | RVA | Absolute | Note |
|------|-----|----------|------|
| SKILL_ENCRYPT_SEND | 0x4D70 | 0x00404D70 | encrypt+send the packet |
| SKILL_BUILDER_INIT | 0x4B40 | 0x00404B40 | init builder object (0x81C bytes) |
| SKILL_BUILDER_SET_HEADER | 0x4B90 | 0x00404B90 | set header + opcode |
| SKILL_BUILDER_ADD_DATA | 0x50D0 | 0x004050D0 | add data bytes (called 4x chained: skillH, skillL, targetH, targetL) |
| SKILL_BUILDER_DESTROY | 0x4B60 | 0x00404B60 | destroy builder |
| SIMPLEMODULUS_ENCRYPT | 0x598420 | 0x00998420 | game's SM::Encrypt (ECX = SM object) |
| SIMPLEMODULUS_DECRYPT | 0x598490 | 0x00998490 | game's SM::Decrypt |

## Slot index
- The **slot index is NOT derivable** from the entity index via `& 0x3FF` — you must scan the
  array (`GameMemory::FindObjectByEntityIndex`).
