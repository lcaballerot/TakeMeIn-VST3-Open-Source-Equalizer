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
