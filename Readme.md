# Opiqo — LV2 Plugin Host

## Frontends Available

Opiqo supports multiple user interface frontends:

- **GTK4**: Modern UI, recommended for most users (`opiqo`)
- **GTK3**: Legacy support (`opiqo-gtk3`)
- **GTK2**: For older systems (`opiqo-gtk2`)
- **Xlib/Xaw**: Minimal X11, no GTK dependency (`opiqo-xlib`)

All frontends share the same DSP and plugin engine.

---

## Compile & Install Instructions

### Prerequisites

- CMake ≥ 3.21
- GCC ≥ 9 or Clang ≥ 10
- Development headers for: JACK, LV2, Lilv, Sndfile, Ogg, Vorbis, Opus, FLAC, LAME
- For GTK frontends: GTK4, GTK3, or GTK2 development packages
- For Xlib: X11, Xaw7, Xt, Xmu development packages

### Build (Release)

From the project root:

```sh
# GTK4 (default)
cmake --preset linux-default
cmake --build --preset linux-default

# GTK3
cmake --preset linux-gtk3
cmake --build --preset linux-gtk3

# GTK2
cmake --preset linux-gtk2
cmake --build --preset linux-gtk2

# Xlib/Xaw
cmake --preset linux-xlib
cmake --build --preset linux-xlib
```

### Install

```sh
# Replace <build-dir> with one of: build-linux, build-linux-gtk3, build-linux-gtk2, build-linux-xlib
cmake --build <build-dir> --target install
# Or from inside the build dir:
make install
```

---

## Usage

Run the desired frontend binary:

```sh
# GTK4
opiqo
# GTK3
opiqo-gtk3
# GTK2
opiqo-gtk2
# Xlib/Xaw
opiqo-xlib
```

### Features
- Host up to 4 LV2 plugins in a chain
- JACK audio backend
- Real-time parameter control
- Recording to WAV/MP3/OGG/OPUS/FLAC
- Named preset management
- JACK port selection

See `README-gtk4.md` and `docs/` for detailed design and advanced usage.
