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

set SOURCES=src\dllmain.cpp src\Logger.cpp src\PacketSniffer.cpp src\GameMemory.cpp src\InlineHook.cpp src\TcpClient.cpp src\PacketHandler.cpp src\MessageRegistry.cpp src\MonsterTracker.cpp src\AttackModule.cpp src\debug\DebugTools.cpp src\debug\DebugScans.cpp src\SkillHook.cpp src\SimpleModulusCipher.cpp src\ViewportMap.cpp src\SkillMap.cpp src\crypto\MuCipher.cpp src\NameRewriter.cpp src\PacketReassembler.cpp

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
