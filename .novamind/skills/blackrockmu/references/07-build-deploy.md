# 7. Build & deploy

```powershell
# byp.dll (C++)
cd gg_bypass2
.\build.bat          # → ..\dist\byp.dll + deploy to F:\blackrockmu\

# opengl32.dll (proxy)
cd gg_bypass2\opengl
.\build.bat          # → ..\..\dist\opengl32.dll

# MuProxy2 (C#)
cd MuProxy2\MuProxy2
dotnet build MuProxy2.csproj -c Release   # → bin\Release\net8.0-windows\MuProxy2.dll

# full deploy
cd gg_bypass2
.\deploy.bat         # builds everything and copies to F:\blackrockmu\
```

- Game dir (`F:\blackrockmu\`) must contain: `byp.dll`, `opengl32.dll`, `MuProxy2.exe` + deps.
- Logs: byp.dll writes a **single** `gg_bypass2.log` in the game dir when `Config::LOG_TO_FILE`
  is true (system + packet logs together; `byp_packets.log` was removed). See §11 for how to read it.
