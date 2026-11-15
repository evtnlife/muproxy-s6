# byp

Código-fonte e análise do bypass do cliente MU BlackRock (`byp.dll`, C++) e da
interface de controle `MuProxy2` (C#).

> Conhecimento consolidado do projeto (arquitetura, canais TCP, criptografia,
> pacote de ataque, object array, build) está na skill **`blackrockmu`**. Carregue-a
> antes de editar código em `gg_bypass2/` ou `MuProxy2/`.

## Estrutura

```
byp/
├── dist/                  # DLLs prontas para deploy (byp.dll, opengl32.dll)
├── docs/                  # Referências RE (attack_packet_re.md é a fonte de verdade)
├── Release/               # Saída de deploy para o jogo
└── gg_bypass2/            # Código-fonte C++ do bypass
    ├── src/               # byp.dll (PacketSniffer, AttackModule, GameMemory, …)
    ├── opengl/            # Proxy OpenGL32 (wrapper DLL que carrega byp.dll)
    ├── build/             # Objetos de compilação
    ├── build.bat
    └── deploy.bat
```

## Build rápido

```powershell
# byp.dll (C++)
cd gg_bypass2
.\build.bat          # → ..\dist\byp.dll + deploy para F:\blackrockmu\

# opengl32.dll (proxy)
cd gg_bypass2\opengl
.\build.bat          # → ..\..\dist\opengl32.dll

# MuProxy2 (C#)
cd MuProxy2\MuProxy2
dotnet build MuProxy2.csproj -c Release

# deploy completo
cd gg_bypass2
.\deploy.bat         # builda tudo e copia para F:\blackrockmu\
```

## Feature flag

Em `gg_bypass2/src/include/Config.h`, a chave mestra `BYPASS_ENABLED` ativa ou
desativa todo o bypass:

```cpp
constexpr bool BYPASS_ENABLED = true;   // bypass ativo
// constexpr bool BYPASS_ENABLED = false; // análise sem interferência
```

Quando `false`, nenhum hook é instalado, permitindo análise comparativa do processo.
