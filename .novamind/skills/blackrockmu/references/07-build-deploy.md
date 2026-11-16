# 7. Build & deploy

```powershell
# byp.dll (C++)
cd gg_bypass2
.\build.bat          # → ..\dist\byp.dll + deploy to F:\blackrockmu\ (kills main.exe first)

# opengl32.dll (proxy)
cd gg_bypass2\opengl
.\build.bat          # → ..\..\dist\opengl32.dll

# MuProxy2 (C#)
cd MuProxy2\MuProxy2
dotnet build MuProxy2.csproj -c Release   # → bin\Release\net8.0-windows\MuProxy2.dll

# full deploy
cd gg_bypass2
.\deploy.bat         # builds everything and copies to F:\blackrockmu\

# imports check (does the game really load our opengl32?)
cd gg_bypass2
.\check_imports.bat  # dumpbin /IMPORTS on main.exe + BlackrockMU.exe
```

- Game dir (`F:\blackrockmu\`) must contain: `byp.dll`, `opengl32.dll`, `MuProxy2.exe` + deps.
- Logs: byp.dll writes a **single** `gg_bypass2.log` in the game dir when `Config::LOG_TO_FILE`
  is true (system + packet logs together; `byp_packets.log` was removed). See §11 for how to read it.

## Source layout rules (the build is explicit, not globbed)
- `build.bat` lists **every** `.cpp` in the `SOURCES` variable, grouped one line per domain
  folder (`src\core\…`, `src\crypto\…`, `src\net\…`, `src\net\winsock\…`, `src\packets\…`,
  `src\game\…`, `src\game\GameMemory\…`, `src\attack\…`, `src\debug\…`).
- **A new `.cpp` that is not listed there is silently NOT compiled.** After creating a file,
  add it to the right line of `SOURCES`.
- The only include path is `/Isrc\include` → include headers by bare filename
  (`#include "MuCipher.h"`), never by relative path.
- No loose `.cpp` in `src/` root — each implementation lives in its domain folder (§12).
- Build output: `..\dist\byp.dll`, then copied to `F:\blackrockmu\byp.dll`.
- `build.bat` also copies `..\dist\opengl32.dll` to the game dir — the opengl build must have
  been run at least once, otherwise that copy fails (the byp.dll copy still happens).
