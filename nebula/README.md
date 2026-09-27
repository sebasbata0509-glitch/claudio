# Nebula

Sintetizador VST3 para EDM atmosférico y melódico: pads, reese, brillos, texturas y vocal chops, con mucha reverb.
Hecho con JUCE 9.0.2 y CMake.

## Compilar en Windows (Visual Studio 2022)

Necesitas CMake 3.22 o superior y Visual Studio 2022 con "Desarrollo para el escritorio con C++".

```bat
cd nebula
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

El plugin queda en `build\Nebula_artefacts\Release\VST3\Nebula.vst3`. Cópialo a
`C:\Program Files\Common Files\VST3\`. También puedes configurar con `-DNEBULA_COPY_AFTER_BUILD=ON` y compilar
desde una consola de administrador para que se copie solo.

Opciones:

- `-DFETCHCONTENT_SOURCE_DIR_JUCE=C:\ruta\a\JUCE` usa una copia local de JUCE en vez de descargarla.
- `-DNEBULA_WERROR=ON` trata los warnings como errores. El CI lo usa.

La carpeta `build\Nebula_artefacts\Release\Standalone` trae además una versión standalone, útil para probar sin DAW.

**CI:** cada push que toca `nebula/` compila Windows x64 y Linux, corre los tests y pluginval (strictness 10), y sube el
VST3 compilado como artefacto del workflow `nebula` en la pestaña *Actions* de GitHub.

## Licencia

JUCE 8+ tiene licencia dual, AGPLv3 o comercial. Si distribuyes binarios de Nebula, tienes que publicar el código
bajo AGPLv3 o tener una licencia de JUCE.
