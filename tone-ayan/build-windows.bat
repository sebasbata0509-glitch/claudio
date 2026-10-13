@echo off
rem Builds Tone-ayan (VST3 + Standalone + tests + CLI) with Visual Studio 2022 and CMake.
rem Run from a "Developer Command Prompt for VS 2022" or any prompt where cmake is on PATH.
setlocal
cd /d "%~dp0"

cmake -S . -B build -A x64 || goto :error
cmake --build build --config Release --parallel || goto :error
ctest --test-dir build -C Release --output-on-failure || goto :error

echo.
echo ==== Listo ====
echo VST3:       %~dp0build\src\plugin\ToneAyan_artefacts\Release\VST3\Tone-ayan.vst3
echo Standalone: %~dp0build\src\plugin\ToneAyan_artefacts\Release\Standalone\Tone-ayan.exe
echo CLI:        %~dp0build\tools\Release\tone-ayan-cli.exe
echo.
echo Para instalar el VST3 copia la carpeta Tone-ayan.vst3 a "C:\Program Files\Common Files\VST3\"
echo (o vuelve a configurar con -DTONEAYAN_COPY_AFTER_BUILD=ON desde una consola de administrador).
exit /b 0

:error
echo La compilacion fallo.
exit /b 1
