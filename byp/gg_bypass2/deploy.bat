@echo off
setlocal EnableDelayedExpansion

:: Forca diretorio de trabalho para a pasta do script
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
cd /d "%ROOT%"

set "SRC=..\dist"
set "MU=F:\blackrockmu"

echo.
echo === Deploy gg_bypass2 ===

call .\build.bat
if errorlevel 1 exit /b 1

call .\opengl\build.bat
if errorlevel 1 exit /b 1

if not exist "%SRC%\byp.dll" (
    echo ERRO: %SRC%\byp.dll nao encontrado.
    exit /b 1
)
if not exist "%SRC%\opengl32.dll" (
    echo ERRO: %SRC%\opengl32.dll nao encontrado.
    exit /b 1
)

if not exist "%MU%" (
    echo ERRO: Pasta do Mu nao encontrada: %MU%
    exit /b 1
)

copy /Y "%SRC%\byp.dll" "%MU%\byp.dll"
if !ERRORLEVEL! NEQ 0 (
    echo ERRO: Falha ao copiar byp.dll.
    exit /b 1
)

copy /Y "%SRC%\opengl32.dll" "%MU%\opengl32.dll"
if !ERRORLEVEL! NEQ 0 (
    echo ERRO: Falha ao copiar opengl32.dll.
    exit /b 1
)

echo SUCESSO: byp.dll e opengl32.dll copiados para %MU%
echo.

endlocal
