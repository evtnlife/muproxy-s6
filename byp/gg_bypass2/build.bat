@echo off
setlocal EnableDelayedExpansion

:: Forca diretorio de trabalho para a pasta do script
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
cd /d "%ROOT%"

:: Configura o ambiente do Visual Studio (C++ x86)
if exist "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
)

echo.
echo Compilando gg_bypass2.dll...

if not exist build mkdir build
if not exist ..\dist mkdir ..\dist

set SOURCES=src\core\dllmain.cpp src\core\Logger.cpp src\core\InlineHook.cpp src\core\OriginalApi.cpp src\crypto\MuCipher.cpp src\crypto\SimpleModulusCipher.cpp src\net\CommandChannel.cpp src\net\PacketChannel.cpp src\net\PacketHandler.cpp src\net\MessageRegistry.cpp src\packets\PacketSniffer.cpp src\packets\PacketReassembler.cpp src\packets\NameRewriter.cpp src\packets\PacketInjector.cpp src\game\GameMemory.cpp src\game\MonsterTracker.cpp src\game\ViewportMap.cpp src\game\SkillMap.cpp src\game\SkillHook.cpp src\attack\AttackModule.cpp src\debug\DebugTools.cpp src\debug\DebugScans.cpp

cl.exe /nologo /O2 /LD /MD /W3 /Isrc\include %SOURCES% /Fobuild\ /link /DEF:src\Main.def /OUT:..\dist\byp.dll /MACHINE:X86 /OPT:REF /OPT:ICF

if !ERRORLEVEL! EQU 0 (
    echo.
    echo ==============================================
    echo SUCESSO: byp.dll criada em ..\dist\byp.dll!
    echo ==============================================
    move dllmain.lib build\ >nul 2>&1
    move dllmain.exp build\ >nul 2>&1

    echo.
    echo Deploy para F:\blackrockmu\...
    taskkill /f /im main.exe >nul 2>&1
    ping -n 3 127.0.0.1 >nul
    copy /y ..\dist\byp.dll F:\blackrockmu\byp.dll
    copy /y ..\dist\opengl32.dll F:\blackrockmu\opengl32.dll
    echo DEPLOY OK
) else (
    echo.
    echo ==============================================
    echo ERRO: Falha na compilacao da DLL.
    echo ==============================================
)

endlocal
