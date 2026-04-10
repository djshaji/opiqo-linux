# Opiqo — GTK4 Implementation Design

> Version: 0.8.0 "Jag-Stang"  
> This document is the canonical reference for the GTK4 frontend.  
> It is intended to serve as a porting base for GTK3, GTK2, Xlib, and ncurses frontends.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Class Hierarchy & Ownership](#2-class-hierarchy--ownership)
3. [Widget Tree](#3-widget-tree)
4. [Class Reference](#4-class-reference)
   - 4.1 [MainWindow](#41-mainwindow)
   - 4.2 [ControlBar](#42-controlbar)
   - 4.3 [PluginSlot](#43-pluginslot)
   - 4.4 [ParameterPanel](#44-parameterpanel)
   - 4.5 [PluginDialog](#45-plugindialog)
   - 4.6 [SettingsDialog](#46-settingsdialog)
   - 4.7 [AppSettings](#47-appsettings)
   - 4.8 [AudioEngine](#48-audioengine)
   - 4.9 [JackPortEnum](#49-jackportenum)
5. [Signal & Callback Flow](#5-signal--callback-flow)
6. [Threading Model](#6-threading-model)
7. [Startup & Shutdown Sequence](#7-startup--shutdown-sequence)
8. [Plugin Cache](#8-plugin-cache)
9. [Recording Flow](#9-recording-flow)
10. [Preset Import / Export](#10-preset-import--export)
11. [Porting Notes](#11-porting-notes)

---

## 1. Architecture Overview

Opiqo is a real-time LV2 plugin host that routes JACK audio through a chain of up to four simultaneously active plugins. The application is split into two layers:

**Domain layer** (toolkit-independent, lives in `src/`):
- `LiveEffectEngine` — LV2 plugin chain management, preset serialisation.
- `AudioEngine` — JACK client, real-time DSP dispatch.
- `FileWriter` — Audio file encoding (WAV / MP3 / OGG / OPUS / FLAC).
- `LV2Plugin` — Single LV2 plugin instance wrapper.
- `AppSettings` — XDG-compliant JSON settings persistence.
- `JackPortEnum` — JACK port discovery and hot-plug notification.

**Frontend layer** (GTK4-specific, lives in `src/gtk4/`):
- `MainWindow` — Application window; owns all other frontend objects.
- `ControlBar` — Horizontal bar at the bottom: power, gain, recording controls.
- `PluginSlot` — One of four plugin slots in the 2×2 grid.
- `ParameterPanel` — Dynamically built LV2 control parameter widgets inside a slot.
- `PluginDialog` — Modal searchable LV2 plugin browser.
- `SettingsDialog` — Two-tab dialog: JACK port selection and preset management.

The frontend communicates with the domain layer exclusively through public method calls and `std::function` callbacks. There is no GTK dependency in the domain layer.

---

## 2. Class Hierarchy & Ownership

```
GtkApplication
└── MainWindow                         (owns everything below)
    ├── LiveEffectEngine               (domain)
    ├── AudioEngine                    (domain; holds LiveEffectEngine*)
    ├── JackPortEnum                   (domain)
    ├── AppSettings                    (value type, saved on ~MainWindow)
    ├── ControlBar                     (unique_ptr)
    ├── PluginSlot[4]                  (raw ptr array, deleted in ~MainWindow)
    │   └── ParameterPanel             (raw ptr, owned by PluginSlot)
    └── SettingsDialog                 (unique_ptr, created lazily)

Dialogs (transient, heap-allocated, self-destroy):
    PluginDialog                       (new/delete by onAddPlugin)
```

**Ownership rules:**
- `MainWindow` holds `unique_ptr` for long-lived objects and raw pointers for the four `PluginSlot` instances (deleted explicitly in the destructor).
- `PluginDialog` is heap-allocated with `new` in `onAddPlugin` and deleted in the confirm/cancel callback closure.
- `SettingsDialog` is created lazily on the first "Settings" click and persists for the lifetime of `MainWindow`.
- `ParameterPanel` is created by `PluginSlot`'s constructor and deleted by `PluginSlot`'s destructor (default).

---

## 3. Widget Tree

```
GtkApplicationWindow  (window_)
├── GtkHeaderBar
│   ├── [start] GtkButton "Test"       (debugBtn, hidden)
│   ├── [title] GtkLabel "Opiqo"
│   └── [end]   GtkButton "About"
│               GtkButton "Settings"
│
└── GtkBox (root, VERTICAL)
    ├── GtkGrid (slotGrid_, 2×2, homogeneous)
    │   ├── [0,0] PluginSlot 1 → GtkFrame
    │   │         └── GtkBox (VERTICAL)
    │   │             ├── GtkBox (headerBox_, HORIZONTAL)
    │   │             │   ├── GtkLabel (nameLabel_)
    │   │             │   ├── GtkButton "+ Add"
    │   │             │   ├── GtkToggleButton "Bypass"
    │   │             │   └── GtkButton "× Remove"
    │   │             ├── GtkSeparator
    │   │             └── GtkScrolledWindow
    │   │                 └── GtkBox (ParameterPanel box_)
    │   │                     └── [per port] GtkBox (row)
    │   │                         ├── GtkLabel (port name)
    │   │                         └── GtkScale | GtkDropDown | GtkCheckButton |
    │   │                                       GtkButton "Fire" | GtkButton "Browse…"
    │   ├── [1,0] PluginSlot 2  (same structure)
    │   ├── [0,1] PluginSlot 3  (same structure)
    │   └── [1,1] PluginSlot 4  (same structure)
    │
    ├── GtkSeparator (horizontal)
    │
    └── ControlBar → GtkBox (bar_, HORIZONTAL)
        ├── GtkToggleButton "Power"
        ├── GtkSeparator
        ├── GtkLabel "Gain"
        ├── GtkScale (gainScale_, 0.0–2.0)
        ├── GtkSeparator
        ├── GtkLabel "Format"
        ├── GtkDropDown (formatDrop_: WAV/MP3/OGG/OPUS/FLAC)
        ├── GtkBox (qualityBox_, hidden for lossless)
        │   ├── GtkLabel "Quality"
        │   └── GtkDropDown (qualityDrop_, 0–9)
        ├── GtkSeparator
        ├── GtkToggleButton "⏺ Record"
        ├── GtkBox (spacer, hexpand)
        ├── GtkLabel (xrunLabel_)
        └── GtkLabel (statusLabel_)
```

---

## 4. Class Reference

### 4.1 MainWindow

**File:** `src/gtk4/MainWindow.h` / `MainWindow.cpp`

The top-level application window. Created once in `main_linux.cpp` and attached to `GtkApplication`. Owns the entire object graph.

#### Constructor: `MainWindow(GtkApplication* app)`

1. Constructs domain objects: `LiveEffectEngine`, `AppSettings::load()`, `JackPortEnum`, `AudioEngine`.
2. Attempts `loadPluginCache()`; on failure calls `engine_->initPlugins()` then `savePluginCache()`.
3. Registers JACK hot-plug callback via `portEnum_->setChangeCallback`.
4. Registers audio error callback via `audio_->setErrorCallback`.
5. Creates `GtkApplicationWindow`, sets title "Opiqo", default size 960×680.
6. Calls `loadCss()` then `buildWidgets()`.
7. Installs a 200 ms periodic timer: `g_timeout_add(200, pollEngineState, this)`.

#### Destructor: `~MainWindow()`

1. Removes the timer (`g_source_remove`).
2. Stops recording if active.
3. Stops the audio engine.
4. Saves settings.
5. Deletes the four `PluginSlot` raw pointers.

#### Private Methods

| Method | Purpose |
|---|---|
| `loadCss()` | Creates a `GtkCssProvider` from the embedded `kAppCss` string literal and registers it for the default display at `APPLICATION` priority. |
| `buildWidgets()` | Constructs the full widget tree (header bar, slot grid, separator, control bar). Wires all slot callbacks. |
| `onPowerToggled(bool on)` | `on=true`: resolves JACK port names, calls `audio_->start()`; `on=false`: stops recording if active, calls `audio_->stop()`. |
| `onGainChanged(float gain)` | Writes to `*engine_->gain` (atomic float ptr) and saves to `settings_.gain`. |
| `onRecordToggled(bool start, int format, int quality)` | Generates a timestamped file path in the XDG Music folder, opens a file descriptor, calls `engine_->startRecording()` or `engine_->stopRecording()`. |
| `onAddPlugin(int slot)` | Guards: engine must be `Running`. Shows `GtkMessageDialog` if not. Calls `engine_->getAvailablePlugins()`, shows `PluginDialog`, and on confirm calls `engine_->addPlugin()` then `slots_[slot-1]->onPluginAdded()`. |
| `onDeletePlugin(int slot)` | Calls `engine_->deletePlugin(slot)` and `slots_[slot-1]->onPluginCleared()`. |
| `onBypassPlugin(int slot, bool bypassed)` | Calls `engine_->setPluginEnabled(slot, !bypassed)`. |
| `onSetValue(int slot, uint32_t portIndex, float value)` | Calls `engine_->setValue(slot, portIndex, value)`. |
| `onSetFilePath(int slot, const std::string& uri, const std::string& path)` | Calls `engine_->setFilePath(slot, uri, path)`. |
| `openSettings()` | Lazily creates `SettingsDialog` (if not yet created), wires its three callbacks, then calls `settingsDlg_->show()`. |
| `onSettingsApply(AppSettings)` | Stores new settings; if engine was running, stops and restarts it with new port names. |
| `onExportPreset()` | Returns `engine_->getPresetList()` (a JSON string) to `SettingsDialog`. |
| `onImportPreset(path)` | Opens the file, parses JSON array, applies each slot via `engine_->applyPreset()` and refreshes the `PluginSlot` UI. |
| `pollEngineState(gpointer)` | **Static**, called every 200 ms on the main thread. Checks `audio_->state()`; resets power/record UI on error. Updates xrun counter. Returns `G_SOURCE_CONTINUE`. |
| `setStatus(msg)` | Logs the message and forwards it to `controlBar_->setStatusText()`. |
| `showAboutDialog()` | Creates and shows a `GtkAboutDialog` with version, codename, auth and website. |
| `savePluginCache()` | Serialises `engine_->getAvailablePlugins()` to JSON and writes to `$XDG_CONFIG_HOME/opiqo/opiqo_plugin_cache.json`. |
| `loadPluginCache()` | Reads the cache file, sets `engine_->pluginInfo`, calls `lilv_world_load_all` and `lilv_world_get_all_plugins`. Returns `false` on any error. |
| `testPluginLoadUnload()` | Debug utility: iterates all available plugins, loads each into slot 1 via `engine_->addPlugin()`, and immediately removes it via `engine_->deletePlugin()`. |

---

### 4.2 ControlBar

**File:** `src/gtk4/ControlBar.h` / `ControlBar.cpp`

A horizontal `GtkBox` appended to the root box by its constructor. Encapsulates all transport and recording controls.

#### Construction

`ControlBar(GtkWidget* parent_box)` calls `buildWidgets()` and appends `bar_` to `parent_box`.

#### Widgets

| Widget | Type | Signal | Action |
|---|---|---|---|
| `powerToggle_` | `GtkToggleButton` | `toggled` | `onPowerToggled()` → fires `powerCb_(active)` |
| `gainScale_` | `GtkScale` (0.0–2.0, step 0.01) | `value-changed` | `onGainChanged()` → fires `gainCb_(value)` |
| `formatDrop_` | `GtkDropDown` (WAV/MP3/OGG/OPUS/FLAC) | `notify::selected` | `onFormatChanged()` — shows/hides `qualityBox_` |
| `qualityDrop_` | `GtkDropDown` (0–9) | — | Read on record toggle |
| `recordToggle_` | `GtkToggleButton` | `toggled` | `onRecordToggled()` → fires `recordCb_(active, fmt, qual)` |
| `xrunLabel_` | `GtkLabel` | — | Updated by `setXrunCount()` |
| `statusLabel_` | `GtkLabel` | — | Updated by `setStatusText()` |

#### Signal Suppression

`suppressSignals_` (bool) is set to `true` around programmatic state changes (e.g. `setPowerState`, `setRecordingActive`) to prevent re-entrant callbacks when the UI is updated by the engine rather than the user.

#### Public State Setters

| Method | Purpose |
|---|---|
| `setPowerState(bool on)` | Sets `powerToggle_` active state without firing `powerCb_`. |
| `setRecordingActive(bool active)` | Sets `recordToggle_` active state without firing `recordCb_`. |
| `setStatusText(const std::string&)` | Updates `statusLabel_` text. |
| `setXrunCount(uint64_t n)` | Formats and updates `xrunLabel_` text. |

#### Quality Visibility Logic

`qualityBox_` is shown only for lossy formats (MP3=1, OGG=2, OPUS=3). For WAV=0 and FLAC=4 it is hidden. This is recalculated on every `notify::selected` on `formatDrop_`.

---

### 4.3 PluginSlot

**File:** `src/gtk4/PluginSlot.h` / `PluginSlot.cpp`

Represents one of four plugin processing slots. Slot indices are 1-based (1–4). The widget is a `GtkFrame` containing a header row and a `ParameterPanel`.

#### Construction

`PluginSlot(int slot, GtkWidget* parent_window)`:
1. Allocates a `ParameterPanel` (passing `parent_window` for file dialog parenting).
2. Calls `buildWidgets()`.

#### Widgets

| Widget | Initial State | Signal | Action |
|---|---|---|---|
| `nameLabel_` | `"Slot N"` | — | Display only; updated by `onPluginAdded` |
| `addButton_` | Always enabled | `clicked` | Fires `addCb_(slot_)` |
| `bypassButton_` | Insensitive | `toggled` | Fires `bypassCb_(slot_, active)` |
| `deleteButton_` | Insensitive | `clicked` | Fires `deleteCb_(slot_)` |
| `paramPanel_->widget()` | Empty scroll area | — | Managed by `ParameterPanel` |

#### State Transitions

**Empty → Loaded** (`onPluginAdded(name, ports)`):
- Sets `nameLabel_` text to plugin name.
- Enables `bypassButton_` and `deleteButton_`.
- Resets bypass toggle to inactive.
- Calls `paramPanel_->build(ports)`.

**Loaded → Empty** (`onPluginCleared()`):
- Resets `nameLabel_` to `"Slot N"`.
- Disables `bypassButton_` and `deleteButton_`.
- Resets bypass toggle to inactive.
- Calls `paramPanel_->clear()`.

#### Callback Wiring

`ParameterPanel` callbacks are adapted here:

```
ParameterPanel::valueCb(portIndex, value)
    → PluginSlot::valueCb_(slot_, portIndex, value)
    → MainWindow::onSetValue(slot, portIndex, value)
    → engine_->setValue(slot, portIndex, value)

ParameterPanel::fileCb(uri, path)
    → PluginSlot::fileCb_(slot_, uri, path)
    → MainWindow::onSetFilePath(slot, uri, path)
    → engine_->setFilePath(slot, uri, path)
```

---

### 4.4 ParameterPanel

**File:** `src/gtk4/ParameterPanel.h` / `ParameterPanel.cpp`

Dynamically constructs GTK controls for each LV2 control port of the loaded plugin. Hosted inside a `GtkScrolledWindow`.

#### Construction

`ParameterPanel(GtkWidget* parent_window)`:
- Creates `scroll_` (`GtkScrolledWindow`, vertical auto-scroll, never horizontal).
- Creates `box_` (`GtkBox`, vertical, spacing 2), set as child of `scroll_`.

#### `build(const std::vector<LV2Plugin::PortInfo>& ports)`

Calls `clear()`, then for each port emits one row:

```
GtkBox (row, HORIZONTAL)
├── GtkLabel (port.label, width 150px, left-aligned)
└── [control widget]
```

Control widget selection by `port.type`:

| `ControlType` | Condition | Widget | Signal | Callback |
|---|---|---|---|---|
| `Float` | `isEnum == true` | `GtkDropDown` | `notify::selected` | `valueCb_(portIndex, scalePoints[idx].first)` |
| `Float` | `isEnum == false` | `GtkScale` (min–max, step=(max-min)/200) | `value-changed` | `onScaleChanged` → `valueCb_` |
| `Toggle` | — | `GtkCheckButton` | `toggled` | `onToggleChanged` → `valueCb_(portIndex, 1.0 or 0.0)` |
| `Trigger` | — | `GtkButton "Fire"` | `clicked` | `onTriggerClicked` → `valueCb_(portIndex, 1.0)` |
| `AtomFilePath` | — | `GtkButton "Browse…"` | `clicked` | `onBrowseClicked` → opens `GtkFileDialog` async |

#### `clear()`

Removes all child widgets from `box_` and frees all `ControlData*` entries in `controlDataList_`.

#### Heap-allocated Callback Data

Each control allocates a `ControlData` struct (or `EnumData` for enums) on the heap. These are pushed into `controlDataList_` and freed in `clear()`. This avoids dangling lambda captures when the panel is rebuilt.

#### File Browse Flow (`AtomFilePath`)

1. `onBrowseClicked` allocates a `BrowseCtx{panel, writableUri}`.
2. Opens `GtkFileDialog::open()` asynchronously.
3. In the async result callback, retrieves the file path with `g_file_get_path`.
4. Fires `fileCb_(writableUri, path)`.
5. Frees `BrowseCtx`.

---

### 4.5 PluginDialog

**File:** `src/gtk4/PluginDialog.h` / `PluginDialog.cpp`

A modal, searchable plugin browser. Heap-allocated by `MainWindow::onAddPlugin` and self-destroys in `onConfirm` / `onCancel`.

#### Construction

`PluginDialog(GtkWindow* parent, const json& plugins)`:
1. Iterates the JSON map; builds a `PluginEntry{uri, name, author}` for each entry.
2. Sorts entries alphabetically by name.
3. Calls `buildWidgets()`.

#### Widget Structure

```
GtkWindow (dialog_, modal, transient, 540×480)
└── GtkBox (vbox, VERTICAL)
    ├── GtkSearchEntry (search_)
    ├── GtkScrolledWindow
    │   └── GtkListBox (listBox_, SINGLE selection)
    │       └── [per plugin] GtkListBoxRow
    │               └── GtkBox (VERTICAL)
    │                   ├── GtkLabel (plugin name, plugin-name CSS class)
    │                   └── GtkLabel (author • URI, dim-label CSS class, ellipsized)
    └── GtkBox (btnBox, HORIZONTAL)
        ├── GtkBox (spacer, hexpand)
        ├── GtkButton "Cancel"
        └── GtkButton "Add Plugin" (suggested-action)
```

Plugin URIs are stored on row boxes via `g_object_set_data_full(rowBox, "plugin-uri", ...)`.

#### Signals

| Source | Signal | Handler |
|---|---|---|
| `search_` | `search-changed` | `onSearchChanged()` → `rebuildList(text)` |
| `listBox_` | `row-activated` | `onConfirm()` (double-click / Enter) |
| Cancel button | `clicked` | `onCancel()` → `gtk_window_destroy` |
| Add Plugin button | `clicked` | `onConfirm()` |

#### `rebuildList(filter)`

Clears all rows. For each entry in `allPlugins_`, performs a case-insensitive substring match against both name and URI. Matching entries are appended as `GtkListBoxRow`.

#### `onConfirm()`

1. Gets the selected `GtkListBoxRow`.
2. Retrieves `"plugin-uri"` data from the row's child widget.
3. Fires `confirmCb_(uri)`.
4. Destroys the window (`gtk_window_destroy`).

---

### 4.6 SettingsDialog

**File:** `src/gtk4/SettingsDialog.h` / `SettingsDialog.cpp`

A two-tab `GtkNotebook` dialog for JACK port configuration and preset management. Created lazily; persists for the lifetime of `MainWindow`.

#### Tabs

**Audio tab** (`buildAudioTab`):

| Control | Type | Purpose |
|---|---|---|
| Capture L | `GtkDropDown` | Left JACK capture port |
| Capture R | `GtkDropDown` | Right JACK capture port |
| Playback L | `GtkDropDown` | Left JACK playback port |
| Playback R | `GtkDropDown` | Right JACK playback port |
| Sample Rate | `GtkLabel` (read-only) | Populated by `updateAudioInfo()` |
| Block Size | `GtkLabel` (read-only) | Populated by `updateAudioInfo()` |
| Apply | `GtkButton` | `onApply()` → `applyCb_` |
| Delete Plugin Cache | `GtkButton` | Deletes `opiqo_plugin_cache.json` |

**Presets tab** (`buildPresetsTab`):

| Control | Type | Purpose |
|---|---|---|
| Export Preset… | `GtkButton` | `onExport()` → `GtkFileDialog::save()` async → writes JSON |
| Import Preset… | `GtkButton` | `onImport()` → `GtkFileDialog::open()` async → fires `importCb_` |

#### `onApply()`

Reads selected port names from the four `GtkDropDown` widgets via `selectedPort()`, constructs a new `AppSettings`, and fires `applyCb_`.

#### `updateAudioInfo(sampleRate, blockSize)`

Updates `srLabel_` and `bsLabel_` text labels. Called by `MainWindow` after the engine starts.

---

### 4.7 AppSettings

**File:** `src/gtk4/AppSettings.h` / `AppSettings.cpp`

Plain data struct with JSON (de)serialisation via `nlohmann::json`. Persisted at `$XDG_CONFIG_HOME/opiqo/settings.json` (fallback: `~/.config/opiqo/settings.json`).

#### Fields

| Field | Type | Default | Purpose |
|---|---|---|---|
| `capturePort` | `string` | `""` | JACK capture port L |
| `capturePort2` | `string` | `""` | JACK capture port R |
| `playbackPort` | `string` | `""` | JACK playback port L |
| `playbackPort2` | `string` | `""` | JACK playback port R |
| `recordFormat` | `int` | `0` (WAV) | Recording file format index |
| `recordQuality` | `int` | `5` | Codec quality 0–9 |
| `gain` | `float` | `1.0` | Linear output gain |

#### Static Methods

- `AppSettings::load()` — Reads and parses the JSON file. Returns default-constructed instance on any error (file not found, parse error, type mismatch).
- `AppSettings::configPath()` — Returns the resolved absolute path to `settings.json`.

---

### 4.8 AudioEngine

**File:** `src/gtk4/AudioEngine.h` / `AudioEngine.cpp`

JACK client lifecycle manager. Uses a Pimpl (`Impl`) to hide JACK headers.

#### States

```
Off → Starting → Running → Stopping → Off
                         ↘ Error
```

#### Thread Safety

- All public methods must be called from the GTK main thread.
- `state_`, `sampleRate_`, `blockSize_`, `xrunCount_` are `std::atomic`.
- `jack_process_callback` runs on the JACK real-time thread; the only GTK interaction is via `g_idle_add` to fire the error callback.

#### Key Methods

| Method | Effect |
|---|---|
| `start(cap1, cap2, pb1, pb2)` | Opens JACK client, registers ports, connects to named physical ports; returns `false` on failure. |
| `stop()` | Closes JACK client cleanly. |
| `state()` | Returns current `State` (atomic read). |
| `xrunCount()` | Returns cumulative JACK xrun count (atomic read). |
| `setErrorCallback(fn)` | Sets a `std::function<void(std::string)>` called on the main thread when JACK fails. |

---

### 4.9 JackPortEnum

**File:** `src/gtk4/JackPortEnum.h` / `JackPortEnum.cpp`

Opens a short-lived, separate JACK client (distinct from `AudioEngine`'s client) for port discovery. Does not participate in audio processing.

#### `enumerateCapturePorts()` / `enumeratePlaybackPorts()`

Returns `std::vector<PortInfo>` where each entry has:
- `id` — Full JACK port name (e.g. `"system:capture_1"`).
- `friendlyName` — Short name for display (e.g. `"capture_1"`).
- `isDefault` — `true` for the first port in the list.

#### Hot-plug

Registers JACK port registration/unregistration callbacks. Fires the stored `std::function<void()>` via `g_idle_add` on the GTK main thread to notify `MainWindow`.

#### `resolveOrDefault(ports, saved)`

If `saved` matches any `id` in `ports`, returns it. Otherwise returns `ports[0].id` or `""`.

---

## 5. Signal & Callback Flow

### Power Toggle

```
User clicks powerToggle_
→ ControlBar::onPowerToggled()
→ powerCb_(true/false)
→ MainWindow::onPowerToggled(on)
  → [on=true]  audio_->start(cap1, cap2, pb1, pb2)
               setStatus("Engine running …")
  → [on=false] audio_->stop()
               setStatus("Engine stopped")
```

### Add Plugin

```
User clicks "+ Add" on PluginSlot N
→ PluginSlot::addCb_(N)
→ MainWindow::onAddPlugin(N)
  → [not Running] show GtkMessageDialog; return
  → new PluginDialog(window_, plugins)
  → dlg->show(confirmCb)
     User selects plugin, clicks "Add Plugin"
     → PluginDialog::onConfirm()
     → confirmCb(uri)
     → engine_->addPlugin(N, uri)
     → slots_[N-1]->onPluginAdded(name, ports)
       → nameLabel_ updated
       → bypassButton_ / deleteButton_ enabled
       → paramPanel_->build(ports)
     → delete dlg
```

### Parameter Change (continuous)

```
User adjusts GtkScale in ParameterPanel
→ ParameterPanel::onScaleChanged(range, cd)
→ valueCb_(portIndex, value)          [ParameterPanel level]
→ PluginSlot::valueCb_(slot, portIndex, value)
→ MainWindow::onSetValue(slot, portIndex, value)
→ engine_->setValue(slot, portIndex, value)
→ LV2Plugin writes value to port buffer (real-time safe)
```

### Record Toggle

```
User clicks "⏺ Record"
→ ControlBar::onRecordToggled()
→ recordCb_(active, fmt, qual)
→ MainWindow::onRecordToggled(start, format, quality)
  → [start=true]
    generate timestamped filename in XDG Music dir
    open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644) → fd
    engine_->startRecording(fd, format, quality)
  → [start=false]
    engine_->stopRecording()
```

### Engine Error (from JACK thread)

```
JACK real-time thread detects error
→ g_idle_add fires on GTK main thread
→ errorCallback(msg)
→ setStatus("Audio error: " + msg)
→ controlBar_->setPowerState(false)

pollEngineState (200 ms timer) also checks:
→ state == Error
→ controlBar_->setPowerState(false)
→ stop recording if active
```

---

## 6. Threading Model

| Thread | Runs | GTK calls allowed |
|---|---|---|
| GTK main thread | All UI code, callbacks, timer, idle handlers | Yes |
| JACK real-time thread | `jack_process_callback` inside `AudioEngine` | **No** — only `g_idle_add` |

All `std::function` callbacks registered on domain objects fire on the GTK main thread. Cross-thread communication from JACK to GTK uses `g_idle_add` exclusively. Shared state between the two threads (`state_`, `sampleRate_`, `blockSize_`, `xrunCount_`) uses `std::atomic`.

---

## 7. Startup & Shutdown Sequence

### Startup

1. `main_linux.cpp` creates `GtkApplication`.
2. `activate` signal: `new MainWindow(app)` → `gtk_widget_show(window)`.
3. `MainWindow` constructor runs (see §4.1).
4. GTK main loop starts.

### Shutdown

1. User closes window → `delete-event` → `MainWindow` destructor.
2. Destructor: stop timer, stop recording, `audio_->stop()`, `settings_.save()`, delete slots.
3. `GtkApplication` exits.

---

## 8. Plugin Cache

LV2 plugin scanning via `lilv` is slow (hundreds of milliseconds). The cache avoids rescanning on every launch.

**Save** (`savePluginCache`):
- Calls `engine_->getAvailablePlugins()` → `nlohmann::json`.
- Writes to `$XDG_CONFIG_HOME/opiqo/opiqo_plugin_cache.json` (creates directory if needed).

**Load** (`loadPluginCache`):
- Reads and parses the JSON file.
- Sets `engine_->pluginInfo` directly (bypasses lilv scan).
- Still calls `lilv_world_load_all` and `lilv_world_get_all_plugins` (needed for plugin instantiation).
- Returns `false` on any error; caller falls back to full scan.

**Invalidation**: The Settings dialog provides a "Delete Plugin Cache" button which calls `deletePluginCache()` — a free function that removes the file. The cache is rebuilt on the next application launch.

---

## 9. Recording Flow

1. User selects format in `formatDrop_` (WAV/MP3/OGG/OPUS/FLAC).
2. For lossy formats, `qualityBox_` becomes visible; user selects quality 0–9.
3. User clicks "⏺ Record".
4. `MainWindow::onRecordToggled(true, fmt, qual)`:
   - Guards: engine must be `Running`.
   - Resolves output directory: `g_get_user_special_dir(G_USER_DIRECTORY_MUSIC)` with `g_get_home_dir()` fallback.
   - Generates filename: `opiqo-YYYYMMDD-HHMMSS.<ext>`.
   - Opens the file with `open(O_WRONLY|O_CREAT|O_TRUNC, 0644)` → file descriptor.
   - Calls `engine_->startRecording(fd, format, quality)`.
   - Sets `isRecording_ = true`, updates status label.
5. User clicks "⏺ Record" again (or engine is stopped/errors):
   - Calls `engine_->stopRecording()`.
   - Sets `isRecording_ = false`.

---

## 10. Preset Import / Export

### Export

1. User clicks "Export Preset…" in Settings dialog.
2. `SettingsDialog::onExport()` opens `GtkFileDialog::save()` asynchronously.
3. On completion, calls `exportCb_()` → `MainWindow::onExportPreset()` → `engine_->getPresetList()` (JSON string).
4. Writes the string to the chosen file path.

### Import

1. User clicks "Import Preset…" in Settings dialog.
2. `SettingsDialog::onImport()` opens `GtkFileDialog::open()` asynchronously.
3. On completion, calls `importCb_(path)` → `MainWindow::onImportPreset(path)`.
4. Reads and parses JSON array (one object per slot, up to 4).
5. For each slot: `engine_->applyPreset(slot, slotJson)`.
6. Refreshes slot UI: calls `onPluginCleared()` or `onPluginAdded(name, ports)`.

---

## 11. Porting Notes

The following section documents GTK4-specific constructs and their equivalents for target toolkits.

### Callback Pattern

All signal connections in this codebase use `g_signal_connect_swapped` with a stateless lambda cast `+[](T* self) { self->method(); }`. This pattern is portable to GTK3/GTK2 as-is. For Xlib/ncurses, replace with direct function calls driven by an event loop.

### Container Layout

| GTK4 widget | GTK3 equivalent | GTK2 equivalent | Xlib/ncurses |
|---|---|---|---|
| `GtkBox` | `GtkBox` | `GtkHBox` / `GtkVBox` | manual coordinates / panel rows |
| `GtkGrid` | `GtkGrid` | `GtkTable` | manual tiling |
| `GtkFrame` | `GtkFrame` | `GtkFrame` | border drawing |
| `GtkScrolledWindow` | `GtkScrolledWindow` | `GtkScrolledWindow` | scrolling viewport |
| `GtkHeaderBar` | `GtkHeaderBar` | — (use `GtkMenuBar`) | title line |

### Dialogs

| GTK4 | GTK3/GTK2 | Notes |
|---|---|---|
| `GtkMessageDialog` | `GtkMessageDialog` | Same API in GTK3; deprecated in GTK4 but still works |
| `GtkAboutDialog` | `GtkAboutDialog` | Same API |
| `GtkFileDialog` (async) | `GtkFileChooserDialog` (sync or async) | GTK3 uses `GtkFileChooserDialog`; GTK2 same |
| Custom `GtkWindow` + `GtkListBox` (PluginDialog) | `GtkDialog` + `GtkTreeView` | GTK2: `GtkListStore` + `GtkTreeView` |

### Drop-downs

| GTK4 | GTK3 | GTK2 |
|---|---|---|
| `GtkDropDown` + `GtkStringList` | `GtkComboBoxText` | `GtkComboBox` |

### CSS Styling

GTK4 CSS (`GtkCssProvider`) has no equivalent in Xlib or ncurses. For GTK3, the same `GtkCssProvider` API is available. For Xlib, use `XChangeWindowAttributes` / drawing primitives. For ncurses, use color pairs and attributes.

### Async File Dialog

`GtkFileDialog::open()` is GTK 4.10+. Under GTK3/GTK2, use the synchronous `gtk_dialog_run()` on `GtkFileChooserDialog`. For ncurses, implement a simple path-input widget.

### Signal Suppression

The `suppressSignals_` bool in `ControlBar` prevents feedback loops when programmatically setting toggle state. In GTK3/GTK2 the same pattern works. In Xlib or ncurses, avoid by calling the underlying setter directly rather than simulating user events.

### Thread Safety

The `g_idle_add` pattern for marshalling JACK callbacks back to the main thread is GTK-specific. For Xlib, use a pipe or `XSendEvent` to wake the event loop. For ncurses, use a condition variable or a self-pipe trick to interrupt `getch()` / `select()`.
