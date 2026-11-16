# Test case: rebuild-after-offset-change

Realistic prompt a user would actually type:

> Troquei o main.exe por uma build nova e agora o monster list tá vazio e o ataque não
> acerta nada. O que eu preciso checar e rebuildar?

## What a correct answer needs

- Recognize that all offsets in `GameAddresses.h` / `GameObject.h` are **ex603-specific**
  (skill §5, §10); a new main.exe invalidates every offset (object-array base, GameObject
  fields, etc.).
- Say the monster list is empty because the ObjectArray scan (§5) is reading the wrong
  addresses → no objects parsed.
- List the rebuild steps: `gg_bypass2/build.bat` (byp.dll) and `gg_bypass2/opengl/build.bat`
  (opengl32.dll), then `deploy.bat` to copy to the game dir (§7).
- Mention re-validating offsets in the ex603 build before assuming the code is wrong.
- Should NOT suggest the C# side is at fault (the scan lives in byp.dll).
- Should NOT propose moving the code around: the source layout is per-domain folders
  (`src/game/GameMemory/*`, `src/include/*`) — §12.
