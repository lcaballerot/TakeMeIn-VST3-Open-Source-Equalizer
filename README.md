# TakeMeIn EQ16 — Open-Source 16-Band Parametric Equalizer (VST3)

A fully custom, premium-grade 16-band parametric EQ built with the VST3 SDK and VSTGUI.  
Designed for professional mixing and mastering workflows.

---

## Features

- **16 parametric bands** — fixed center frequencies from 25 Hz to 20 kHz
- **Per-band gain** — ±24 dB range per band with smooth biquad peaking filters
- **Per-band Q control** — Q from 0.1 to 10.0, adjustable via scroll wheel or drag
- **DJ-style fader caps** — custom-drawn graphite faders with grip ridges and glow accent
- **EQ response curve** — real-time combined frequency response overlay
- **Curve-drag editing** — click and drag directly on the response curve to adjust band gain
- **Q ring visualization** — arc drawn around the fader cap on hover to show bandwidth
- **Output gain** — master ±12 dB fader with CLIP LED (2.5s sticky latch on peak ≥ 0 dBFS)
- **A/B compare** — two independent EQ slots (A and B) with one-click swap and copy
- **Solo per band** — Ctrl+click any fader to solo that band through a bandpass filter (+6 dB makeup)
- **Session autosave** — last-used EQ settings restored automatically on every plugin launch
- **Profile system** — save, load, export, and import named `.tmieq` presets
- **5 factory presets** — Vocal Clarity, Bass Boost, Air, De-Mud, Loudness Smile
- **UI scaling** — 75%, 100%, and 125% zoom levels, persisted between sessions
- **Adaptive framerate** — 60 fps when the plugin window is focused, 15 fps when unfocused
- **Premium UI** — animated RGB squared outline, graphite gradient panels, cyan/amber accent palette

---

## Requirements

- **Windows 10/11** (x64)
- **Visual Studio 2022** (Community or higher) with the C++ Desktop workload
- **CMake 3.15+** (bundled with Visual Studio)
- A VST3-compatible DAW (Ableton Live, FL Studio, Reaper, Bitwig, etc.)

---

## Building

```bat
build.bat
```

The script:
1. Calls `vcvars64.bat` to set up the MSVC toolchain
2. Runs CMake build (`--target TakeMeInEQ`)
3. Installs the compiled `.vst3` bundle to `C:\Program Files\Common Files\VST3\`

> **First-time setup** — configure the CMake project once before building:
> ```bat
> cmake -S . -B build -DSMTG_CREATE_PLUGIN_LINK=0
> ```

---

## File Locations

| Path | Purpose |
|------|---------|
| `C:\Program Files\Common Files\VST3\TakeMeInEQ.vst3` | Installed plugin |
| `%APPDATA%\TakeMeIn\EQ16\last_session.tmieq` | Auto-saved last session |
| `%APPDATA%\TakeMeIn\EQ16\Profiles\*.tmieq` | User-saved profiles |
| `%APPDATA%\TakeMeIn\EQ16\ui.cfg` | UI scale setting |

---

## Project Structure

```
source/
  eqcids.h        — Parameter IDs, band frequencies, gain/Q ranges
  biquad.h        — Peaking biquad filter math
  eqprocessor.*   — DSP: biquad cascade, solo bandpass, output gain, peak metering
  eqcontroller.*  — VST3 controller: parameter registration, state save/load, UI scale
  eqeditor.*      — Custom VSTGUI editor: all drawing, interaction, animation
  eqpersist.*     — File-based state persistence (.tmieq format, factory presets)
  eqentry.cpp     — VST3 module entry point
  version.h       — Version constants
resource/
  win32resource.rc
external/
  CMakeLists.txt  — VST3 SDK fetch target
CMakeLists.txt
build.bat         — One-click build + install script
```

---

## Band Center Frequencies

| # | Frequency | # | Frequency |
|---|-----------|---|-----------|
| 1 | 25 Hz | 9 | 1 kHz |
| 2 | 40 Hz | 10 | 1.6 kHz |
| 3 | 63 Hz | 11 | 2.5 kHz |
| 4 | 100 Hz | 12 | 4 kHz |
| 5 | 160 Hz | 13 | 6.3 kHz |
| 6 | 250 Hz | 14 | 10 kHz |
| 7 | 400 Hz | 15 | 16 kHz |
| 8 | 630 Hz | 16 | 20 kHz |

---

## License

Source code released under the **MIT License**.  
The VST3 SDK is © Steinberg Media Technologies GmbH and is governed by its own [license](https://www.steinberg.net/vst3sdk).

---

Made by **LCaballerot01**

---
---

# TakeMeIn EQ16 — Ecualizador Paramétrico de 16 Bandas de Código Abierto (VST3)

Ecualizador paramétrico de 16 bandas de alta calidad, construido desde cero con el VST3 SDK y VSTGUI.  
Diseñado para flujos de trabajo de mezcla y masterización profesionales.

---

## Características

- **16 bandas paramétricas** — frecuencias centrales fijas de 25 Hz a 20 kHz
- **Ganancia por banda** — rango de ±24 dB por banda con filtros biquad de pico
- **Control de Q por banda** — Q de 0.1 a 10.0, ajustable con la rueda del ratón o arrastrando
- **Faders estilo DJ** — faders personalizados con degradado grafito, ranuras de agarre y acento luminoso
- **Curva de respuesta EQ** — superposición de la respuesta en frecuencia combinada en tiempo real
- **Edición arrastrando la curva** — haz clic y arrastra directamente sobre la curva para ajustar la ganancia
- **Visualización del anillo Q** — arco dibujado alrededor del fader al pasar el cursor, indicando el ancho de banda
- **Ganancia de salida** — fader maestro ±12 dB con LED de CLIP (latch de 2.5s al alcanzar 0 dBFS)
- **Comparación A/B** — dos slots de EQ independientes (A y B) con cambio y copia en un clic
- **Solo por banda** — Ctrl+clic en cualquier fader para escuchar solo esa banda (filtro bandpass +6 dB)
- **Autoguardado de sesión** — los ajustes de EQ utilizados por última vez se restauran automáticamente al abrir el plugin
- **Sistema de perfiles** — guarda, carga, exporta e importa presets `.tmieq` con nombre
- **5 presets de fábrica** — Vocal Clarity, Bass Boost, Air, De-Mud, Loudness Smile
- **Escalado de interfaz** — zoom al 75%, 100% y 125%, guardado entre sesiones
- **Framerate adaptativo** — 60 fps cuando la ventana del plugin está activa, 15 fps cuando no lo está
- **Interfaz premium** — contorno RGB cuadrado animado, paneles con degradado grafito, paleta de colores cyan/ámbar

---

## Requisitos

- **Windows 10/11** (x64)
- **Visual Studio 2022** (Community o superior) con el módulo de Desarrollo en C++
- **CMake 3.15+** (incluido con Visual Studio)
- Un DAW compatible con VST3 (Ableton Live, FL Studio, Reaper, Bitwig, etc.)

---

## Compilación

```bat
build.bat
```

El script:
1. Llama a `vcvars64.bat` para configurar el compilador MSVC
2. Ejecuta la compilación con CMake (`--target TakeMeInEQ`)
3. Instala el bundle `.vst3` compilado en `C:\Program Files\Common Files\VST3\`

> **Configuración inicial** — configura el proyecto CMake una vez antes de compilar:
> ```bat
> cmake -S . -B build -DSMTG_CREATE_PLUGIN_LINK=0
> ```

---

## Ubicación de Archivos

| Ruta | Propósito |
|------|-----------|
| `C:\Program Files\Common Files\VST3\TakeMeInEQ.vst3` | Plugin instalado |
| `%APPDATA%\TakeMeIn\EQ16\last_session.tmieq` | Última sesión autoguardada |
| `%APPDATA%\TakeMeIn\EQ16\Profiles\*.tmieq` | Perfiles guardados por el usuario |
| `%APPDATA%\TakeMeIn\EQ16\ui.cfg` | Configuración de escala de interfaz |

---

## Estructura del Proyecto

```
source/
  eqcids.h        — IDs de parámetros, frecuencias de banda, rangos de ganancia/Q
  biquad.h        — Matemática del filtro biquad de pico
  eqprocessor.*   — DSP: cascada biquad, bandpass de solo, ganancia de salida, medición de pico
  eqcontroller.*  — Controlador VST3: registro de parámetros, carga/guardado de estado, escala UI
  eqeditor.*      — Editor VSTGUI personalizado: todo el dibujo, interacción y animación
  eqpersist.*     — Persistencia de estado en archivos (formato .tmieq, presets de fábrica)
  eqentry.cpp     — Punto de entrada del módulo VST3
  version.h       — Constantes de versión
resource/
  win32resource.rc
external/
  CMakeLists.txt  — Descarga del VST3 SDK
CMakeLists.txt
build.bat         — Script de compilación e instalación en un clic
```

---

## Frecuencias Centrales de Bandas

| # | Frecuencia | # | Frecuencia |
|---|-----------|---|-----------|
| 1 | 25 Hz | 9 | 1 kHz |
| 2 | 40 Hz | 10 | 1.6 kHz |
| 3 | 63 Hz | 11 | 2.5 kHz |
| 4 | 100 Hz | 12 | 4 kHz |
| 5 | 160 Hz | 13 | 6.3 kHz |
| 6 | 250 Hz | 14 | 10 kHz |
| 7 | 400 Hz | 15 | 16 kHz |
| 8 | 630 Hz | 16 | 20 kHz |

---

## Licencia

Código fuente publicado bajo la **Licencia MIT**.  
El VST3 SDK es © Steinberg Media Technologies GmbH y está sujeto a su propia [licencia](https://www.steinberg.net/vst3sdk).

---

Desarrollado por **LCaballerot01**
