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
echo === Build OpenGL Proxy ===

if not exist Release mkdir Release
if not exist ..\..\dist mkdir ..\..\dist

cl.exe /nologo /O2 /LD /MD /W3 /EHsc /DUNICODE /D_UNICODE /DNDEBUG /DWIN32 /D_WINDOWS /D_USRDLL /DGLPROXY_EXPORTS opengl_proxy.cpp /FoRelease\ /link /DEF:opengl_proxy.def /OUT:..\..\dist\opengl32.dll /MACHINE:X86 /OPT:REF /OPT:ICF OpenGL32.lib user32.lib

if !ERRORLEVEL! EQU 0 (
    echo.
    echo ==============================================
    echo SUCESSO: opengl32.dll criada em ..\..\dist\opengl32.dll!
    echo ==============================================
    move opengl_proxy.exp Release\ >nul 2>&1
    move opengl_proxy.lib Release\ >nul 2>&1
    move Release\opengl_proxy.obj Release\ >nul 2>&1
) else (
    echo.
    echo ==============================================
    echo ERRO: Falha na compilacao do OpenGL proxy.
    echo ==============================================
)

endlocal
