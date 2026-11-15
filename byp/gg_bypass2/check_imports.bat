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

echo --- main.exe imports ---
dumpbin /IMPORTS "f:\blackrockmu\main.exe" | findstr /i "opengl startup kernel32"
echo --- BlackrockMU.exe imports ---
dumpbin /IMPORTS "f:\blackrockmu\BlackrockMU.exe" | findstr /i "opengl startup kernel32"
endlocal
