# MuProxy2

UI de controle (C# .NET 8 WinForms) para o bypass `byp.dll` do cliente MU.
MuProxy2 **nunca** toca em criptografia — quem decripta é o `byp.dll`.

> Conhecimento completo do projeto (arquitetura, canais, cripto, pacote de ataque,
> object array, build) está na skill **`blackrockmu`**. Este README só resume o papel
> do MuProxy2 no fluxo.

## Como o byp.dll é carregado (não é injeção)

O jogo (`main.exe`) carrega naturalmente `opengl32.dll` da pasta do jogo
(`F:\blackrockmu\`). Esse `opengl32.dll` é um wrapper GLProxy
(`gg_bypass2/opengl/opengl_proxy.cpp`) cujo `DllMain` faz
`LoadLibraryA("byp.dll")` e depois repassa as chamadas GL para o
`opengl32.dll` real do System32. O `byp.dll` hooka `ws2_32` e abre a
comunicação TCP com o MuProxy2.

- **Não** usa `CreateRemoteThread`, nem named pipe, nem `MuProxy2.Hook.dll`.
  (Esse caminho antigo foi descartado.)

## Componentes

- `MuProxy2.exe` — UI WinForms: Packet List, Monster List, Logs, controles de ataque.
- `HookListenerService` — escuta as portas TCP **44412** (CMD) e **44413** (PKT).
- `DllCommandService` — roteia as mensagens `TAG|payload` via `MessageRegistry`.
- `TcpLoopbackProxy` — relay cego (MITM) das portas **44411** (server list) e
  **55902** (game server). **Nunca** inspeciona, decripta ou modifica payloads.
- `MonsterService` — rastreia monstros/posição do player.
- `AttackService` — controla o ataque automático.

## Canais

| Porta | Nome | Direção | Conteúdo |
|-------|------|---------|----------|
| 44412 | CMD | bidirecional | comandos, entities (`ENTITY`), logs (`LOG`) — formato `TAG\|payload` |
| 44413 | PKT | byp.dll → MuProxy2 | pacotes **já decriptados** (`PKT\|SEND\|<hex>` / `PKT\|RECV\|<hex>`) |
| 44411 | Server List | relay cego | tráfego bruto (não inspecionado) |
| 55902 | Game Server | relay cego | tráfego bruto (não inspecionado) |

A Packet List é alimentada pelo canal **44413**, não pelo proxy.

## Build

```powershell
cd MuProxy2\MuProxy2
dotnet build MuProxy2.csproj -c Release
# binário em bin\Release\net8.0-windows\MuProxy2.dll
```

Deploy completo (byp.dll + opengl32.dll + MuProxy2): `gg_bypass2\deploy.bat`.
