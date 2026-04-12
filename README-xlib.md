# opiqo-linux (Xlib/Xaw)

Native Linux port of the **Opiqo LV2 plugin host** — a four-slot real-time
guitar/audio effects processor with a minimal Xlib/Xaw desktop UI and a JACK audio backend.

---

## Features

- **4 independent plugin slots** arranged in a 2×2 grid
- **LV2 plugin browser** with live text filtering across all installed plugins
- **Dynamic parameter panels** — controls generated at runtime from plugin port metadata
- **JACK audio backend** — full-duplex stereo, RT-safe callback, compatible with classic JACK2 and PipeWire-JACK
- **Recording** to WAV, MP3, OGG, OPUS, or FLAC while all four slots are active
- **Bypass** and **delete** per slot without audio glitches
- **Settings dialog** — JACK port routing and preset import/export
- **XDG-compliant settings** stored in `~/.config/opiqo/settings.json`
- Minimal X11 dependencies (no GTK required)

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Xlib/Xaw UI Layer                  │
│  MainWindow  PluginSlot[0-3]  PluginDialog          │
│  ControlBar  ParameterPanel   SettingsDialog        │
└─────────────────────┬───────────────────────────────┘
                      │  std::function<> callbacks
┌─────────────────────▼───────────────────────────────┐
│               Audio Platform Layer                  │
│  AudioEngine (JACK client)   JackPortEnum           │
│  AppSettings (XDG config)                           │
└─────────────────────┬───────────────────────────────┘
                      │  LiveEffectEngine::process()
┌─────────────────────▼───────────────────────────────┐
│                  DSP Host Layer                     │
│  LiveEffectEngine   LV2Plugin   FileWriter          │
│  LockFreeQueue      AudioBuffer                     │
└─────────────────────────────────────────────────────┘
```

### Thread model

| Thread | Responsibility |
|--------|---------------|
| X11 main thread | All widget events, signal handlers, UI state changes |
| JACK RT callback thread | `AudioEngine::processCb` → `LiveEffectEngine::process()` |
| Cross-thread | `std::atomic<State>` for engine state; self-pipe + event loop marshal |

The JACK callback is allocation-free: all stereo interleave/deinterleave work
buffers are pre-allocated in `AudioEngine::start()`.

---

## Source layout

```
src/
  main_xlib.cpp                ← X11/Xaw entry point
  xlib/
    XlibApp.h/.cpp             ← X11/Xt/Xaw app context, event loop, self-pipe
    AppSettings.h/.cpp         ← XDG config load/save
    AudioEngine.h/.cpp         ← JACK client lifecycle and RT callback
    JackPortEnum.h/.cpp        ← Physical port discovery + hot-plug watch
    MainWindow.h/.cpp          ← Top-level window; owns all domain objects
    ControlBar.h/.cpp          ← Power, Gain, Record, Format, status bar
    PluginSlot.h/.cpp          ← Per-slot frame (header + ParameterPanel)
    PluginDialog.h/.cpp        ← Searchable LV2 plugin browser
    ParameterPanel.h/.cpp      ← Dynamic Xaw controls from LV2 port metadata
    SettingsDialog.h/.cpp      ← Audio routing + preset import/export dialog
    opiqo.css                  ← Application stylesheet (for reference)

  # Shared DSP core (unchanged from Android/Windows build)
  LiveEffectEngine.h/.cpp
  LV2Plugin.hpp
  FileWriter.h/.cpp
  LockFreeQueue.h/.cpp
  AudioBuffer.h
  utils.h
  json.hpp
  logging_macros.h
  lv2_ringbuffer.h
```

### Key design decisions

**Minimal X11 dependencies** — Only Xlib, Xt, Xaw7, Xmu, and Xext are required. No GTK or GLib.

**Self-pipe event loop** — JACK RT thread signals the main thread via a pipe, which is polled in the Xt event loop for thread safety.

**Thin adapter pattern** — The DSP core (`LiveEffectEngine`, `LV2Plugin`, `FileWriter`, `LockFreeQueue`) is shared verbatim with the Android and Windows builds. Only `src/xlib/` and `src/main_xlib.cpp` are X11-specific.

**Lock-free RT boundary** — `AudioEngine` pre-allocates interleaved stereo buffers in `start()` and makes zero heap allocations inside the JACK callback. Plugin parameter updates from the UI go through `LiveEffectEngine::setValue()` which uses the existing `LockFreeQueue` infrastructure.

**Xaw file dialogs** — Uses custom file path entry dialogs (no native file browser).

---

## Prerequisites

### Debian / Ubuntu
```bash
sudo apt install \
  libx11-dev \
  libxaw7-dev \
  libxt-dev \
  libxmu-dev \
  libxext-dev \
  libjack-jackd2-dev \
  liblilv-dev \
  libsndfile1-dev \
  libmp3lame-dev \
  libopus-dev \
  libopusenc-dev \
  libflac-dev \
  libvorbis-dev \
  libogg-dev \
  cmake make pkg-config
```

### Fedora
```bash
sudo dnf install \
  libX11-devel \
  libXaw-devel \
  libXt-devel \
  libXmu-devel \
  libXext-devel \
  jack-audio-connection-kit-devel \
  lilv-devel \
  libsndfile-devel \
  lame-devel \
  opus-devel \
  opusenc-devel \
  flac-devel \
  libvorbis-devel \
  libogg-devel \
  cmake make pkg-config
```

---

## Build

```bash
cmake --preset linux-xlib        # Release build in build-linux-xlib/
cmake --build build-linux-xlib -j$(nproc)
```

For a debug build:
```bash
cmake --preset linux-xlib-debug
cmake --build build-linux-xlib-debug -j$(nproc)
```

---

## Run

```bash
./build-linux-xlib/opiqo-xlib
```

JACK (or PipeWire with `pipewire-jack`) must be running before starting the engine. Use the UI to connect to the default physical ports, or open **Settings** to choose specific capture/playback ports first.

---

## Controls

| Control | Action |
|---------|--------|
| **Power** toggle | Start / stop the JACK audio engine |
| **Gain** slider | Linear output gain (0 – 2×) |
| **+** button (slot) | Open plugin browser for that slot |
| **Bypass** toggle (slot) | Bypass the loaded plugin in real time |
| **×** button (slot) | Remove the loaded plugin |
| **Format** dropdown | Select recording format (WAV / MP3 / OGG / OPUS / FLAC) |
| **Quality** dropdown | Codec quality 0–9 (lossy formats only) |
| **Record** toggle | Start / stop recording to `~/Music/opiqo-<timestamp>.<ext>` |
| Settings | Open Settings dialog |

---

## Settings dialog

**Audio tab** — select JACK capture and playback ports (L + R independently). Sample rate and block size are read-only; they reflect the values reported by the JACK server. Clicking **Apply** restarts the engine with the new port selection.

**Presets tab** — export the current four-slot configuration to a JSON file, or import a previously saved preset to restore all four slots.

---

## Recording

Output files are written to `~/Music/` with a timestamped filename. The engine continues processing plugins without interruption while recording. The file is finalized cleanly on `stopRecording()` even if the application exits during a recording session.

| Format | File | Notes |
|--------|------|-------|
| WAV | `.wav` | Lossless, via libsndfile |
| MP3 | `.mp3` | Lossy, via LAME |
| OGG Vorbis | `.ogg` | Lossy, via libvorbis |
| OPUS | `.opus` | Lossy, via libopusenc |
| FLAC | `.flac` | Lossless, via libFLAC |
