# BlackRockMU — `byp.dll` + `MuProxy2`

Bypass **client-side** do cliente MU Online do servidor privado BlackRockMU
(`145.239.149.84`), dividido em três peças:

| Peça | Linguagem | Papel |
|------|-----------|-------|
| `byp.dll` | C++ (x86) | Vive **dentro** do `main.exe`: hooka `ws2_32`, decripta/criptografa os pacotes, envia/recebe comandos, controla ataque e auto-potion. |
| `opengl32.dll` | C++ (x86) | Wrapper OpenGL na pasta do jogo. O `DllMain` dele faz `LoadLibraryA("byp.dll")` — é assim que o bypass entra no processo. |
| `MuProxy2.exe` | C# (.NET 8, WinForms) | UI de controle: Packet List, Monster List, logs, ataque, auto-potion. Fala TCP com o `byp.dll`. |

> **Não há injeção.** Nada de `CreateRemoteThread`: o jogo carrega o
> `opengl32.dll` da própria pasta naturalmente, e ele carrega o `byp.dll`.

---

## 1. O que o repositório contém (e o que ele NÃO contém)

O git versiona **apenas o código-fonte do projeto**:

```
byp/            fonte do bypass (C++) e do MuProxy2 (C#)
openspec/       especificações de mudança (proposal/design/tasks)
Scripts/        .reg de resolução de tela/idioma do cliente
.agents/        skills de openspec
package.json    tooling de node (openspec)
```

**Fora do git** (baixados/gerados por fora, ver `.gitignore`):

```
Data/                       assets do cliente (~1,1 GB)
BlackrockMU.exe.WebView2/   runtime WebView2 do launcher
main.exe, *.dll,            binários do cliente (inclui o wzAudio.dll do PS — §1.1)
config.EMU, Settings.ini, main.premium
MuEmu-master/, OpenMU-master/   fontes de referência de terceiros
```

> Os arquivos continuam **no disco**; eles só foram removidos do índice do
> git. Se precisar versionar algo de lá de novo:
> `git add -f <caminho>`.

### Baixar o cliente do jogo

O `main.exe`, o `Data/` e o launcher **não vêm com o repositório** — extraia o
cliente do BlackRockMU em `F:\blackrockmu\`. A estrutura final da pasta do
jogo precisa ser:

```
F:\blackrockmu\
├── main.exe            ← do cliente do jogo
├── Data\               ← do cliente do jogo
├── wzAudio.dll         ← do cliente do jogo (OBRIGATÓRIO — ver §1.1)
├── byp.dll             ← buildado por você
├── opengl32.dll        ← buildado por você
└── MuProxy2.exe        ← buildado por você
```

### 1.1 `wzAudio.dll` — onde mora o PS (não apague!)

O `main.exe` **importa `wzAudio.dll` da pasta do jogo** (linkagem estática, não
opcional):

```
dumpbin /IMPORTS main.exe
  → wzAudio.dll : wzAudioCreate, wzAudioOption, wzAudioDestroy,
                  wzAudioGetStreamOffsetRange, wzAudioPlay, wzAudioStop, …
```

Nesse servidor esse arquivo **não é só o decodificador de áudio**: o **PS**
(protector anti-tamper/anti-debug do BlackRockMU) **se disfarça de módulo de
áudio**. Ele exporta as 17 funções `wzAudio*` — para o jogo continuar
funcionando normalmente — e importa `GetStartupInfoA` de `KERNEL32`, que é
exatamente o ponto onde o protector instala o hook de monitoração do processo.

O **bypass do PS é feito nesse arquivo**: o `wzAudio.dll` que fica na pasta do
jogo é a versão que já neutraliza o PS, preservando as exports `wzAudio*` para
o cliente abrir e tocar áudio sem o protector atrapalhando.

> O `byp.dll` **não** tem código de PS hoje: a família `PsBypass`,
> `PsPacketHook`, `DllNotifyHook`, `MainPatcher`, `StartupHook`,
> `AsyncScanner` e `VPTracer` foi removida na Fase 0 (commit `e0945a2`) como
> código morto — nunca entrou no `SOURCES` do `build.bat`. Dá para recuperar o
> código antigo com
> `git show e0945a2^:byp/gg_bypass2/src/PsBypass.cpp`.

**Consequência prática:** o `wzAudio.dll` **precisa continuar na pasta do
jogo** (sem ele o `main.exe` nem abre) e **não precisa estar no git** — é
arquivo do cliente, não do projeto.

---

## 2. Pré-requisitos

| Ferramenta | Versão / detalhe |
|------------|------------------|
| Windows | 10/11 x64 (o jogo é **x86**) |
| Visual Studio | 2022 ou 18 — workload **"Desenvolvimento para desktop em C++"**. Os `.bat` chamam `vcvars32.bat` (toolchain x86) automaticamente. |
| .NET SDK | **8.0** (`dotnet --list-sdks` deve mostrar um 8.x) |
| Cliente do jogo | `main.exe` + `Data/` do BlackRockMU |

---

## 3. Build

Tudo em **PowerShell** ou **cmd**, a partir de `F:\blackrockmu` (raiz do repositório).

### 3.1 `byp.dll` (C++)

```powershell
cd byp\gg_bypass2
.\build.bat          # → byp\dist\byp.dll  (+ copia para F:\blackrockmu\)
```

O `build.bat` compila com `cl.exe /O2 /LD /MD /MACHINE:X86` e **mata o
`main.exe`** antes de copiar. A lista de fontes é explícita: todo `.cpp` novo
precisa ser adicionado na variável `SOURCES` do `build.bat`, senão **não é
compilado**.

### 3.2 `opengl32.dll` (proxy GL)

```powershell
cd byp\gg_bypass2\opengl
.\build.bat          # → byp\dist\opengl32.dll
```

### 3.3 `MuProxy2` (C#)

```powershell
cd byp\MuProxy2\MuProxy2
dotnet build MuProxy2.csproj -c Release
# → byp\MuProxy2\MuProxy2\bin\Release\net8.0-windows\MuProxy2.exe
```

### 3.4 Deploy completo

```powershell
cd byp\gg_bypass2
.\deploy.bat         # builda byp.dll + opengl32.dll e copia para F:\blackrockmu\
```

> O `deploy.bat` só copia `byp.dll` e `opengl32.dll`. O `MuProxy2.exe` você
> copia manualmente para a pasta do jogo.

### 3.5 Testes

```powershell
cd byp\MuProxy2\MuProxy2.Tests
dotnet test -c Release
```

---

## 4. Instalação

1. **Instale o cliente** — extraia `main.exe`, `Data/` e o launcher em
   `F:\blackrockmu\`.
2. **Build** — rode os passos da seção 3 (ou `.\deploy.bat`).
3. **Copie o MuProxy2** — `byp\MuProxy2\MuProxy2\bin\Release\net8.0-windows\*`
   → `F:\blackrockmu\`.
4. **Confira** — a pasta do jogo deve conter `byp.dll`, `opengl32.dll` e
   `MuProxy2.exe` ao lado do `main.exe`.
5. **Verifique os imports** (confirma que o jogo carrega o nosso `opengl32.dll`):

```powershell
cd byp\gg_bypass2
.\check_imports.bat   # dumpbin /IMPORTS em main.exe e BlackrockMU.exe
```

---

## 5. Como usar

1. Abra o **MuProxy2.exe** (na pasta do jogo).
2. Aba **General** → confirme o caminho em *GAME EXECUTABLE*
   (padrão `F:\blackrockmu\main.exe`) e o servidor em *Server* /
   *Port* (`145.239.149.84` / `44411`).
3. **▶ LAUNCH GAME** — o MuProxy2 cria a pasta de sessão, exporta as
   variáveis de ambiente e abre o `main.exe`. O `opengl32.dll` da pasta do
   jogo carrega o `byp.dll`, que se conecta de volta na UI.
4. **START PROXY** — sobe o relay local (`127.0.0.1:44411`) para a lista de
   servidores e o relay do game server (`55902`).
5. Aba **Custom Stuff**:
   - *Monster List* — habilite a captura de monstros;
   - *Auto Potion* — HP/MP em % para o auto-potion;
   - controle de ataque (ataque automático).

### Canais

| Porta | Nome | Direção | Conteúdo |
|-------|------|---------|----------|
| 44411 | Server List | relay cego | tráfego bruto (não inspecionado) |
| 55902 | Game Server | relay cego | tráfego bruto (não inspecionado) |
| 44412 | CMD | bidirecional | comandos, `ENTITY`, `LOG` — formato `TAG\|payload` |
| 44413 | PKT | byp.dll → MuProxy2 | pacotes **já decriptados** (`PKT\|SEND\|<hex>`) |

A **Packet List** é alimentada pelo canal **44413**, não pelo proxy.
O MuProxy2 **nunca** toca em criptografia — quem decripta é o `byp.dll`.

---

## 6. Variáveis de ambiente (handshake MuProxy2 ↔ byp.dll)

| Variável | Quem define | Para que serve |
|----------|--------------|----------------|
| `MUPROXY_BASE` | MuProxy2 (processo) | Base de portas da instância (`0`, `100`, `200`…). O `main.exe` filho herda no `CreateProcess` e o `byp.dll` lê no attach. Sem ela → base 0. |
| `MUPROXY_SESSION` | MuProxy2 (processo) | Pasta da sessão: log do `byp.dll` e `Settings.ini` por instância (evita duas instâncias escreverem no mesmo arquivo). |
| `MUPROXY_HWID` | manual | Força um HWID específico (`XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX`) no pacote de login. |

Cada instância do MuProxy2 cuida de **um** `main.exe`. Instâncias diferentes
usam bases diferentes e se identificam pelo **título da janela** — o `byp`
envia `HELLO|pid=..|base=..` e a UI recusa um `byp` de outra instância.

---

## 7. Solução de problemas

| Sintoma | Causa provável | O que fazer |
|---------|----------------|-------------|
| `Main.exe CRC error!` e o cliente se mata | O cliente valida o CRC do `main.exe` **relativo ao CWD**. A pasta de sessão precisa do arquivo. | O `SessionFolder` já cria *hardlinks* dos arquivos do jogo na pasta da sessão — confira se a criação do link falhou (mesmo volume?). |
| O jogo abre mas o MuProxy2 não recebe nada | O `opengl32.dll` da pasta do jogo não é o nosso, ou o `byp.dll` não foi copiado. | Rode `.\check_imports.bat` e confira as DLLs ao lado do `main.exe`. |
| `byp RECUSADO (pid=…, base=…)` no log | O `main.exe` conectado pertence a outra instância do MuProxy2. | Feche o `main.exe` da outra instância — cada janela cuida de um jogo. |
| Porta já em uso | Outra instância/base ocupa a porta. | Feche a outra instância; a base é escolhida testando o bind real das portas. |
| O jogo conecta direto no servidor, sem passar pelo proxy | `START PROXY` não foi clicado, ou o `main.exe` tem o host do servidor fixo. | Clique em **START PROXY** antes de logar. |
| Log do bypass | `Config::LOG_TO_FILE` ligado → `gg_bypass2.log` na pasta do jogo (ou na pasta da sessão, se `MUPROXY_SESSION` existir). | Ver `byp\gg_bypass2\src\include\Config.h`. |
| O cliente não abre / erro de DLL faltando | O `main.exe` importa `wzAudio.dll` da pasta (§1.1) — ele é obrigatório e também é onde está o bypass do PS. | Garanta que `wzAudio.dll` continua ao lado do `main.exe` (não versionado, mas **obrigatório** em disco). |

---

## 8. Referência

- Conhecimento consolidado do projeto (arquitetura, canais, cripto, pacote de
  ataque, object array) na skill **`blackrockmu`**
  (`.novamind/skills/blackrockmu/`), com um arquivo por tema em `references/`.
- `byp\README.md` e `byp\MuProxy2\README.md` — resumo de cada componente.
- `byp\docs\` — reversing, análise do pacote de ataque, cripto.
