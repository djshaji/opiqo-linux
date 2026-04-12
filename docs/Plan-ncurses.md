# Opiqo — ncurses (Console) UI Implementation Plan

> Version: 0.8.0 "Jag-Stang"  
> Based on the GTK4 canonical design (`design/gtk4.md`) and following the
> conventions established by the Xlib/Xaw port (`src/xlib/`).  
> Domain layer (`LiveEffectEngine`, `AudioEngine`, `FileWriter`, `LV2Plugin`,
> `AppSettings`, `JackPortEnum`) is **reused unchanged**.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Class Hierarchy & Ownership](#2-class-hierarchy--ownership)
3. [Screen Layout](#3-screen-layout)
4. [TUI Widget Model](#4-tui-widget-model)
5. [Class Reference](#5-class-reference)
   - 5.1 [NcursesApp](#51-ncursesapp)
   - 5.2 [MainWindow](#52-mainwindow)
   - 5.3 [ControlBar](#53-controlbar)
   - 5.4 [PresetBar](#54-presetbar)
   - 5.5 [PluginSlot](#55-pluginslot)
   - 5.6 [ParameterPanel](#56-parameterpanel)
   - 5.7 [PluginDialog](#57-plugindialog)
   - 5.8 [SettingsDialog](#58-settingsdialog)
6. [Input Model & Focus Management](#6-input-model--focus-management)
7. [Threading Model](#7-threading-model)
8. [Startup & Shutdown Sequence](#8-startup--shutdown-sequence)
9. [Color & Styling](#9-color--styling)
10. [Keyboard Reference](#10-keyboard-reference)
11. [Build Integration](#11-build-integration)
12. [Porting Differences from Xlib](#12-porting-differences-from-xlib)

---

## 1. Architecture Overview

The ncurses frontend mirrors the two-layer split used by all other frontends:

**Domain layer** (unchanged, in `src/`):  
`LiveEffectEngine`, `AudioEngine`, `FileWriter`, `LV2Plugin`, `AppSettings`, `JackPortEnum`.

**Frontend layer** (ncurses-specific, in `src/ncurses/`):
- `NcursesApp` — analogue of `XlibApp`. Owns the ncurses context, terminal
  restore, self-pipe for cross-thread wakeup, and the main event loop.
- `MainWindow` — owns all TUI components and domain objects; mirrors
  `src/xlib/MainWindow`.
- `ControlBar`, `PresetBar`, `PluginSlot`, `ParameterPanel` — mirrors of their
  Xlib counterparts, drawn into `WINDOW*` sub-windows.
- `PluginDialog`, `SettingsDialog` — full-screen modal overlays using the
  `panel` library.

The frontend communicates with the domain layer exclusively via public method
calls and `std::function` callbacks, exactly as in other frontends. There is no
ncurses dependency in the domain layer.

---

## 2. Class Hierarchy & Ownership

```
NcursesApp
└── MainWindow                         (owns everything below)
    ├── LiveEffectEngine               (domain, unique_ptr)
    ├── AudioEngine                    (domain, unique_ptr)
    ├── JackPortEnum                   (domain, unique_ptr)
    ├── AppSettings                    (value type, saved on ~MainWindow)
    ├── ControlBar                     (unique_ptr)
    ├── PresetBar                      (unique_ptr)
    ├── PluginSlot[4]                  (unique_ptr array)
    │   └── ParameterPanel             (unique_ptr, owned by PluginSlot)
    └── SettingsDialog                 (unique_ptr, created lazily)

Modal screens (stack-allocated inside MainWindow, shown via PANEL*):
    PluginDialog                       (created on demand, destroyed after confirm/cancel)
```

**Ownership rules:**
- `NcursesApp` is created in `main()` and holds the terminal; `MainWindow` is a
  member (not heap-allocated) to guarantee destruction before `NcursesApp`
  restores the terminal.
- `PluginDialog` is a local variable inside `MainWindow::onAddPlugin()`; it
  blocks in its own `runModal()` loop and is destroyed when the function returns.
- `SettingsDialog` is lazily created on the first Settings key and lives for the
  lifetime of `MainWindow`.
- `ParameterPanel` is owned by `PluginSlot` via `unique_ptr`.

---

## 3. Screen Layout

### 3.1 Minimum Terminal Size

| Dimension | Minimum | Recommended |
|---|---|---|
| Columns | 100 | 132 |
| Rows | 34 | 43 |

On resize (`SIGWINCH`) the entire screen is redrawn. If the terminal is too
small, a centered error message is shown and all input is suspended until the
terminal is enlarged.

### 3.2 Region Map (132 × 43 reference)

```
Row  0      : Title bar  ─────────────────────────────────────────────────────
Rows 1–20   : Plugin grid (2 columns × 2 rows, each slot ~ 10 lines tall)
              Col  0–64  : Slot 1 (top-left)  │  Col 65–131 : Slot 2 (top-right)
              Col  0–64  : Slot 3 (bot-left)  │  Col 65–131 : Slot 4 (bot-right)
Row  21     : Horizontal separator ──────────────────────────────────────────
Row  22     : PresetBar  ─────────────────────────────────────────────────────
Row  23     : Horizontal separator ──────────────────────────────────────────
Rows 24–25  : ControlBar (two lines: gains/format on 24, record/status on 25)
Row  26     : Key-hint bar (help line)  ─────────────────────────────────────
```

At a narrower 100-column terminal the slot width is halved but the structure
is identical.

### 3.3 ASCII Art — Main Screen

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Opiqo 0.8.0 "Jag-Stang"                          [S]ettings  [A]bout  [Q]uit│
├─────────────────────────────────┬───────────────────────────────────────────┤
│ [1] Slot 1: — empty —           │ [2] Slot 2: ZamGate                       │
│ [+] Add  [B] Bypass  [X] Remove │ [+] Add  [*BYPASS*]  [X] Remove           │
│ ─────────────────────────────── │ ─────────────────────────────────────────  │
│                                 │  Attack    [====|====]  0.010 ms           │
│                                 │  Release   [======|==]  0.250 ms           │
│                                 │  Threshold [===|=====] -20.0 dB            │
│                                 │  ...                                        │
├─────────────────────────────────┼────────────────────────────────────────────│
│ [3] Slot 3: — empty —           │ [4] Slot 4: — empty —                      │
│ [+] Add  [B] Bypass  [X] Remove │ [+] Add  [B] Bypass  [X] Remove            │
│ ─────────────────────────────── │ ───────────────────────────────────────────│
│                                 │                                             │
├─────────────────────────────────┴────────────────────────────────────────────│
│ Preset: [My Rock Preset          ▼]  Name: [________________] [L]oad [Sa]ve [D]el │
├──────────────────────────────────────────────────────────────────────────────│
│ [P]ower  Gain: [===|=====] 1.00  Format: [WAV▼]               Xruns: 0      │
│ [●Rec]   Status: Engine running                                              │
├──────────────────────────────────────────────────────────────────────────────│
│ Tab:Focus  Enter/Space:Activate  ←→:Adjust  F1:Help  F2:Settings  F10/Q:Quit│
└──────────────────────────────────────────────────────────────────────────────┘
```

### 3.4 PluginDialog (full-screen modal overlay)

```
╔════════════════════════════════════════╗
║  Add Plugin — Search: [_________]      ║
╠════════════════════════════════════════╣
║ ▶ ZamGate                              ║
║   zamaudio — http://zamaudio.com/...   ║
║   ───────────────────────────────────  ║
║   Calf Flanger                         ║
║   Calf Studio — http://calf-studio...  ║
║   ───────────────────────────────────  ║
║   ...                                  ║
╠════════════════════════════════════════╣
║             [Add Plugin]  [Cancel]     ║
╚════════════════════════════════════════╝
```

### 3.5 SettingsDialog (full-screen modal overlay, two tabs)

```
╔═══════════════════ Settings ═══════════════════╗
║ [Audio Ports]  [Presets]                       ║
╠════════════════════════════════════════════════╣
║ Capture L  : [system:capture_1  ▼]             ║
║ Capture R  : [system:capture_2  ▼]             ║
║ Playback L : [system:playback_1 ▼]             ║
║ Playback R : [system:playback_2 ▼]             ║
║ Sample Rate: 48000 Hz                          ║
║ Block Size : 256                               ║
╠════════════════════════════════════════════════╣
║   [Apply]       [Delete Plugin Cache]  [Close] ║
╚════════════════════════════════════════════════╝
```

---

## 4. TUI Widget Model

ncurses has no built-in widget toolkit. A minimal focusable-widget abstraction
is needed.

### 4.1 `Widget` base (internal header `src/ncurses/Widget.h`)

```cpp
class Widget {
public:
    virtual ~Widget() = default;

    // Draw self into parent window. Called on every redraw.
    virtual void draw() = 0;

    // Handle a key event. Return true if consumed.
    virtual bool handleKey(int ch) { return false; }

    // Focus management
    bool isFocused() const { return focused_; }
    virtual void setFocused(bool f) { focused_ = f; draw(); }

    bool isVisible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    bool isEnabled() const { return enabled_; }
    virtual void setEnabled(bool e) { enabled_ = e; draw(); }

protected:
    bool focused_ = false;
    bool visible_ = true;
    bool enabled_ = true;
};
```

Concrete widgets provided:
- `Button` — draws `[Label]`; activates on `Enter` or `Space`.
- `ToggleButton` — draws `[Label]` / `[*LABEL*]`; toggles on `Enter`/`Space`.
- `Slider` — draws `[====|====] value`; `←`/`→` adjust by step; `PgUp`/`PgDn`
  by 10×step; `Home`/`End` jump to min/max.
- `TextEntry` — draws `[__text__]`; typing replaces text; `Enter` commits;
  `Esc` cancels. Activates on `Enter`.
- `DropdownList` — draws `[Current ▼]`; `Enter` opens an inline scrollable list;
  `↑`/`↓` navigate; `Enter` selects.
- `CheckButton` — draws `[x]`/`[ ]`; toggles on `Space`/`Enter`.
- `Label` — non-focusable, display only.

### 4.2 Focus Ring (`FocusRing`)

Each screen region (MainWindow, PluginSlot, ControlBar, PresetBar, dialogs)
maintains a `FocusRing`: an ordered `std::vector<Widget*>`. `Tab` advances
focus; `Shift+Tab` reverses. Focus visually highlights the active widget with
`A_REVERSE` or a color-pair accent.

The global focus ring in `MainWindow` contains the top-level interactive areas
in order: Slot 1 header → Slot 2 header → Slot 3 header → Slot 4 header →
PresetBar widgets → ControlBar widgets. When a slot is focused, `↓` descends
into its parameter panel's own focus ring; `Esc` returns to the slot header.

### 4.3 Redraw strategy

A dirty-flag approach is used:
- Each widget keeps a `dirty_` flag set when its state changes.
- `NcursesApp::redraw()` calls `draw()` on all dirty widgets, then
  `doupdate()` once.
- Full redraw on `SIGWINCH` or `KEY_RESIZE`.

---

## 5. Class Reference

### 5.1 NcursesApp

**File:** `src/ncurses/NcursesApp.h` / `NcursesApp.cpp`

Analogous to `XlibApp`. Owns the ncurses context.

#### Constructor

1. Calls `initscr()`, `cbreak()`, `noecho()`, `keypad(stdscr, TRUE)`.
2. Calls `start_color()` + `use_default_colors()`; initialises all color pairs
   (see §9).
3. Calls `curs_set(0)` (hide cursor except when a `TextEntry` is active).
4. Creates the self-pipe (`pipe2(pipeFd_, O_NONBLOCK)`).
5. Stores terminal `SIGWINCH` handler via `sigaction`.

#### Destructor

1. Closes self-pipe fds.
2. Calls `endwin()` to restore terminal.

#### `run(int timeoutMs = 200)`

The main event loop:

```cpp
void NcursesApp::run(int timeoutMs) {
    while (!quitRequested_) {
        // select() on STDIN_FILENO and selfPipe[0] with timeoutMs
        fd_set rfds;
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(pipeFd_[0], &rfds);
        int maxFd = std::max(STDIN_FILENO, pipeFd_[0]) + 1;

        timeval tv { .tv_sec = timeoutMs / 1000,
                     .tv_usec = (timeoutMs % 1000) * 1000 };
        int r = select(maxFd, &rfds, nullptr, nullptr, &tv);

        if (r < 0 && errno == EINTR) { handleResize(); continue; }

        if (FD_ISSET(pipeFd_[0], &rfds))
            drainPipe();          // runs marshalQueue_ callbacks

        if (timerDue())
            fireTimerCallbacks(); // 200 ms poll tick

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            int ch = getch();
            dispatchKey(ch);
        }

        redraw();
    }
}
```

The `select()` timeout doubles as the 200 ms engine-state poll interval,
replacing `g_timeout_add`.

#### `postToMain(std::function<void()> fn)`

Thread-safe. Appends `fn` to `marshalQueue_` under a mutex, then writes one
byte to `pipeFd_[1]` to wake the `select()`.

#### `void quit()`

Sets `quitRequested_ = true`.

#### Key fields

| Field | Type | Purpose |
|---|---|---|
| `pipeFd_[2]` | `int[2]` | Self-pipe for cross-thread wakeup |
| `marshalMutex_` | `std::mutex` | Guards `marshalQueue_` |
| `marshalQueue_` | `vector<function<void()>>` | Callbacks to run on main thread |
| `quitRequested_` | `bool` | Loop termination flag |
| `focusedScreen_` | `Screen*` | Pointer to currently active screen (main / dialog) |

---

### 5.2 MainWindow

**File:** `src/ncurses/MainWindow.h` / `MainWindow.cpp`

The central coordinator. Holds all domain objects and TUI components.

#### Construction

1. Constructs `LiveEffectEngine`, loads `AppSettings`, constructs `JackPortEnum`, `AudioEngine`.
2. Attempts `loadPluginCache()`; on failure calls `engine_->initPlugins()` then `savePluginCache()`.
3. Registers JACK hot-plug callback via `portEnum_->setChangeCallback` →
   `app_.postToMain(...)`.
4. Registers audio error callback via `audio_->setErrorCallback` →
   `app_.postToMain(...)`.
5. Calls `buildWindows()` — creates `WINDOW*` sub-windows for each region and
   constructs `ControlBar`, `PresetBar`, and four `PluginSlot` objects.
6. Registers the 200 ms timer callback with `NcursesApp`.
7. Calls `loadNamedPresets()`.

#### `buildWindows()`

Calculates sub-window geometry from `COLS` / `LINES`:

```
titleWin_   = newwin(1,  COLS,       0,    0)
slot1Win_   = newwin(H,  COLS/2,     1,    0)
slot2Win_   = newwin(H,  COLS/2,     1,    COLS/2)
slot3Win_   = newwin(H,  COLS/2,     1+H,  0)
slot4Win_   = newwin(H,  COLS/2,     1+H,  COLS/2)
presetWin_  = newwin(1,  COLS,       1+2H+1, 0)
controlWin_ = newwin(3,  COLS,       1+2H+2, 0)
hintWin_    = newwin(1,  COLS,       LINES-1, 0)
```

where `H = (LINES - 6) / 2`.

All sub-windows are wrapped in `PANEL*` via `new_panel()` for proper refresh
ordering.

#### Resize handling (`onResize()`)

Called by `NcursesApp` on `SIGWINCH` / `KEY_RESIZE`:
1. `endwin()` + `refresh()` to pick up new `COLS`/`LINES`.
2. `delwin()` + `del_panel()` for all existing panels.
3. Re-runs `buildWindows()`.
4. Full redraw of all components.

#### Private methods (same semantics as GTK4/Xlib)

| Method | Notes vs. Xlib |
|---|---|
| `onPowerToggled(bool)` | Identical semantics |
| `onGainChanged(float)` | Identical |
| `onRecordToggled(bool, int, int)` | Path prompt via `promptFilePath()` instead of file dialog |
| `onAddPlugin(int)` | Pushes `PluginDialog` onto modal stack |
| `onDeletePlugin(int)` / `onBypassPlugin(int, bool)` | Identical |
| `onSetValue(int, uint32_t, float)` | Identical |
| `onSetFilePath(int, const std::string&, const std::string&)` | Path entered via `TextEntry` inline |
| `openSettings()` | Pushes `SettingsDialog` onto modal stack |
| `pollEngineState()` | Fired by `NcursesApp` timer (200 ms) via registered callback |
| `setStatus(msg)` | Writes to `controlBar_->setStatusText()` |
| `loadPluginCache()` / `savePluginCache()` | Identical to Xlib |
| `loadNamedPresets()` / `saveNamedPresets()` | Identical to Xlib |
| `applyFullPreset(json)` | Identical |
| `onPresetLoad()` / `onPresetSave(name)` / `onPresetDelete()` | Identical |
| `promptFilePath(title, forSave)` | Opens a `TextEntry` overlay to type a file path |

#### `dispatchKey(int ch)`

Routes raw ncurses key codes:

```
Q / F10            → app_.quit()
F2 / S             → openSettings()
F1 / ?             → showHelp()
Tab / Back-Tab     → advanceFocus() / retreatFocus()
1–4                → jumpFocusToSlot(n)
P                  → controlBar_->togglePower()
R                  → controlBar_->toggleRecord()
↑↓←→ / Enter / Space → forward to focusedWidget_->handleKey(ch)
```

---

### 5.3 ControlBar

**File:** `src/ncurses/ControlBar.h` / `ControlBar.cpp`

Drawn in `controlWin_` (3 rows × full-width).

#### Rows

```
Row 0 : [P]ower  Gain: [<====|=====>] 1.00  Format: [WAV      ▼]  Quality: [5▼]
Row 1 : [● Rec]  Status: Engine running …                     Xruns: 3
Row 2 : (blank padding)
```

Row 2 is reserved so the control bar does not merge visually with the hint bar.

#### Widgets (all `Widget` subclasses, laid out with hardcoded column offsets)

| Widget | Type | Columns | Key binding |
|---|---|---|---|
| `powerToggle_` | `ToggleButton "Power"` | 0–8 | `P` (global shortcut) |
| `gainSlider_` | `Slider` (0.0–2.0, step 0.01) | 17–40 | `←`/`→` when focused |
| `formatDrop_` | `DropdownList` (WAV/MP3/OGG/OPUS/FLAC) | 49–62 | `Enter` opens |
| `qualityDrop_` | `DropdownList` (0–9) | 72–78 | hidden for lossless |
| `recordToggle_` | `ToggleButton "● Rec"` | 0–9 (row 1) | `R` (global shortcut) |
| `xrunLabel_` | `Label` | right-aligned row 1 | — |
| `statusLabel_` | `Label` | cols 10–COLS-12 row 1 | — |

#### Suppression

`suppressSignals_` (bool) prevents `powerCb_`/`recordCb_` from firing when
`setPowerState()` / `setRecordingActive()` are called programmatically.

#### Quality visibility

`qualityDrop_` and its label are hidden (not drawn) when `currentFormat_` is
WAV (0) or FLAC (4), exactly mirroring the GTK4 / Xlib logic.

---

### 5.4 PresetBar

**File:** `src/ncurses/PresetBar.h` / `PresetBar.cpp`

Drawn in `presetWin_` (1 row × full-width). Due to the space constraint, all
controls share a single line:

```
Preset: [My Rock Preset              ▼]  Name: [_____________] [L]oad [Sa]ve [D]el
```

#### Widgets

| Widget | Type | Key binding |
|---|---|---|
| `presetDrop_` | `DropdownList` (named preset list) | `Enter` opens |
| `nameEntry_` | `TextEntry` | `Enter` to edit, commit on second `Enter` |
| `loadBtn_` | `Button "Load"` | `Enter` / `L` shortcut |
| `saveBtn_` | `Button "Save"` | `Enter` / `S` shortcut |
| `deleteBtn_` | `Button "Del"` | `Enter` / `D` shortcut |

When `presetDrop_` selection changes, `nameEntry_` is updated automatically
(mimicking `GtkDropDown`→`nameEntry_` synchronisation in GTK4).

#### `setPresetNames(names)`

Replaces the `DropdownList` contents.

---

### 5.5 PluginSlot

**File:** `src/ncurses/PluginSlot.h` / `PluginSlot.cpp`

Drawn inside one quadrant window (`slotWin_[0..3]`). Slots are 1-based.

#### Layout within slot window

```
Row 0 : [N] Slot N: Plugin Name                                   (border row)
Row 1 : [+] Add  [B] Bypass  [X] Remove
Row 2 : ─────────────────────────────────────────────────────────
Rows 3+ : ParameterPanel (scrollable)
```

When no plugin is loaded, rows 3+ are blank.

#### State transitions

Same as GTK4 / Xlib:  
- **Empty → Loaded** (`onPluginAdded`): update name label, enable Bypass/Remove,
  call `paramPanel_->populate(ports)`.  
- **Loaded → Empty** (`onPluginCleared`): reset name, disable Bypass/Remove,
  call `paramPanel_->clear()`.

#### Scroll offset

`scrollOffset_` (int) tracks the first visible parameter row. `↓`/`↑` within
the parameter area adjust it. `PgDn`/`PgUp` scroll by `slotHeight - 3` rows.

#### Callback types

Identical to `src/xlib/PluginSlot.h`:
```cpp
using AddCb     = std::function<void(int slot)>;
using DeleteCb  = std::function<void(int slot)>;
using BypassCb  = std::function<void(int slot, bool bypassed)>;
using SetValCb  = std::function<void(int slot, uint32_t portIdx, float value)>;
using SetFileCb = std::function<void(int slot, uint32_t portIdx, const std::string& path)>;
```

---

### 5.6 ParameterPanel

**File:** `src/ncurses/ParameterPanel.h` / `ParameterPanel.cpp`

Drawn inside the lower portion of a `PluginSlot` window. Each port occupies
exactly one row:

```
 Attack     [======|====]   0.010 ms
 Release    [======|====]   0.250 ms
 Threshold  [====|========] -20.0 dB
 Mode       [Downward ▼]
 Active     [x]
 Reset      [Fire]
 IR File    [Browse…] /path/to/impulse.wav
```

#### `PortDef` struct (same as Xlib `PortDef`):
```cpp
struct PortDef {
    uint32_t    index;
    std::string name;
    PortKind    kind;   // Float, Toggle, Trigger, Enum, AtomFilePath
    float       min, max, def;
    std::vector<std::string> options;  // enum labels
};
```

#### Control rendering per PortKind

| Kind | ncurses rendering | Input |
|---|---|---|
| `Float` (continuous) | `Slider` widget, 20 chars wide, value formatted right | `←`/`→` step; `PgUp`/`PgDn` coarse; `Home`/`End` |
| `Float` (enum/scalePoints) | `DropdownList` | `Enter` opens inline list |
| `Toggle` | `CheckButton` `[x]`/`[ ]` | `Space`/`Enter` |
| `Trigger` | `Button "[Fire]"` | `Enter` |
| `AtomFilePath` | `Button "[Browse…]"` + `Label` showing current path | `Enter` → `promptFilePath()` |

#### Heap-allocated callback data

Each control stores a `ControlData*` (or subtype) on the heap, collected in
`controlDataList_` and freed in `clear()`. Same pattern as GTK4 / Xlib.

#### File path input

`AtomFilePath` ports use `MainWindow::promptFilePath()` rather than a file
dialog. This opens a full-width `TextEntry` overlay at the bottom of the screen
(above the hint bar) with a prompt string. The user types a path and presses
`Enter` to confirm or `Esc` to cancel.

---

### 5.7 PluginDialog

**File:** `src/ncurses/PluginDialog.h` / `PluginDialog.cpp`

A full-screen framed overlay (centred, 60×20 or proportional) displayed using
the `panel` library. Contains its own `runModal()` event loop.

#### Construction

`PluginDialog(NcursesApp& app, const std::string& pluginsJson)`:
1. Parses JSON; builds `vector<PluginEntry>` sorted alphabetically by name.
2. Creates overlay `WINDOW*` + `PANEL*`.
3. Calls `buildWidgets()`.

#### `runModal()` → `std::string`

Runs a nested `select()` event loop focused within the dialog until confirm or
cancel. Returns the chosen URI, or `""` on cancel.

#### Widgets

| Widget | Type | Behaviour |
|---|---|---|
| `searchEntry_` | `TextEntry` | Live filtering as user types — updates list after each keystroke |
| `pluginList_` | Scrollable list (raw `WINDOW*`) | `↑`/`↓` navigate entries; `Enter` = confirm |
| `confirmBtn_` | `Button "Add Plugin"` | Submits selection |
| `cancelBtn_` | `Button "Cancel"` | Returns `""` |

#### `rebuildList(filter)`

Case-insensitive substring match against `name` and `uri`. Rebuilds the visible
entries list. Search is synchronous (no WorkProc needed; ncurses is fast enough
for the plugin count).

#### Layout

```
╔══════════════════ Add Plugin ══════════════════╗
║  Search: [___________________________]         ║
╠════════════════════════════════════════════════╣
║ ▶ Calf Flanger                                 ║
║   Calf Studio     (http://calf-studio.org/)    ║
║  ─────────────────────────────────────────     ║
║   ZamGate                                      ║
║   zamaudio       (http://www.zamaudio.com/)    ║
║   ...                                          ║
╠════════════════════════════════════════════════╣
║         [Add Plugin]        [Cancel]           ║
╚════════════════════════════════════════════════╝
```

URI string is stored in `filteredEntries_[selectedIdx_].uri`.

---

### 5.8 SettingsDialog

**File:** `src/ncurses/SettingsDialog.h` / `SettingsDialog.cpp`

Full-screen framed overlay with two tabs. Contains its own `runModal()` loop;
changes are applied on "Apply" or discarded on "Close".

#### Tabs

**Tab 0 — Audio Ports:**

```
╔═════════════════════ Settings ══════════════════════╗
║ [Audio Ports]  [Presets]                            ║
╠═════════════════════════════════════════════════════╣
║ Capture L  : [system:capture_1   ▼]                ║
║ Capture R  : [system:capture_2   ▼]                ║
║ Playback L : [system:playback_1  ▼]                ║
║ Playback R : [system:playback_2  ▼]                ║
║                                                     ║
║ Sample Rate : 48000 Hz                              ║
║ Block Size  : 256 frames                            ║
╠═════════════════════════════════════════════════════╣
║  [Apply]         [Delete Plugin Cache]    [Close]   ║
╚═════════════════════════════════════════════════════╝
```

**Tab 1 — Presets:**

```
╔═════════════════════ Settings ══════════════════════╗
║ [Audio Ports]  [*Presets*]                          ║
╠═════════════════════════════════════════════════════╣
║                                                     ║
║  [Export Preset…]                                   ║
║  Saves current slot configuration to a JSON file.  ║
║                                                     ║
║  [Import Preset…]                                   ║
║  Loads slot configuration from a JSON file.        ║
║                                                     ║
╠═════════════════════════════════════════════════════╣
║                                         [Close]     ║
╚═════════════════════════════════════════════════════╝
```

#### Tab switching

`Tab` key inside the dialog cycles the two tab buttons. `←`/`→` on a tab button
switch the visible tab page; the focus ring changes accordingly.

#### `onApply()`

Reads `DropdownList` selections, constructs `AppSettings`, fires `applyCb_`.

#### Export / Import

Both use `promptFilePath()` (the `NcursesApp`-level text-entry overlay) to get
a file path, matching the file-dialog role from GTK4.

#### `show()` / `hide()`

Unlike the GTK4 version (which uses `gtk_widget_show`), `show()` enters
`runModal()`. Calling code blocks until the dialog is dismissed.

---

## 6. Input Model & Focus Management

### 6.1 Global key bindings (always active)

| Key | Action |
|---|---|
| `Q` | `app_.quit()` |
| `F10` | `app_.quit()` |
| `F1` / `?` | Show key-binding help overlay |
| `F2` / `S` (capital) | Open SettingsDialog |
| `P` (capital) | Toggle Power |
| `R` (capital) | Toggle Record |
| `1`–`4` | Jump focus to slot N header |
| `Tab` | Advance to next focusable widget |
| `Shift+Tab` (Back-Tab) | Retreat to previous focusable widget |
| `KEY_RESIZE` | Full redraw |

### 6.2 Widget-level key handling

When a widget has focus:

| Widget | Key | Action |
|---|---|---|
| `Button` | `Enter` / `Space` | Activate (fire callback) |
| `ToggleButton` | `Enter` / `Space` | Toggle and fire callback |
| `Slider` | `←` / `→` | ±1 step |
| `Slider` | `PgUp` / `PgDn` | ±10 steps |
| `Slider` | `Home` / `End` | min / max |
| `DropdownList` | `Enter` | Open inline list |
| `DropdownList` (open) | `↑` / `↓` | Navigate |
| `DropdownList` (open) | `Enter` | Select and close |
| `DropdownList` (open) | `Esc` | Close without change |
| `TextEntry` | Printable chars | Insert at cursor |
| `TextEntry` | `Backspace` / `DEL` | Delete char |
| `TextEntry` | `Enter` | Commit text, fire callback |
| `TextEntry` | `Esc` | Discard, restore previous text |
| `CheckButton` | `Space` / `Enter` | Toggle |

### 6.3 Focus order on main screen

```
Title bar (no focus)
→ Slot 1 header (Add / Bypass / Remove)
  → [↓] Slot 1 ParameterPanel rows (local sub-ring, Esc returns)
→ Slot 2 header
  → [↓] Slot 2 ParameterPanel rows
→ Slot 3 header
  → [↓] Slot 3 ParameterPanel rows
→ Slot 4 header
  → [↓] Slot 4 ParameterPanel rows
→ PresetBar: presetDrop_ → nameEntry_ → loadBtn_ → saveBtn_ → deleteBtn_
→ ControlBar: powerToggle_ → gainSlider_ → formatDrop_ → [qualityDrop_] → recordToggle_
→ (wraps back to Slot 1 header)
```

---

## 7. Threading Model

| Thread | Runs | Terminal calls allowed |
|---|---|---|
| Main thread | All TUI code, event loop, timer callbacks | Yes — all ncurses calls |
| JACK real-time thread | `jack_process_callback` inside `AudioEngine` | **No** — only `NcursesApp::postToMain()` |

### Self-pipe protocol

1. JACK thread calls `app_.postToMain(fn)`:
   - Acquires `marshalMutex_`, appends `fn` to `marshalQueue_`.
   - Writes one byte `'\x01'` to `pipeFd_[1]`.
2. Main thread's `select()` unblocks on `pipeFd_[0]`:
   - Reads and discards one byte.
   - Swaps `marshalQueue_` under mutex.
   - Executes all drained callbacks.

This is structurally identical to the Xlib self-pipe (`XlibApp::postToMain`).

### Timer (engine state poll)

`NcursesApp::run()` uses `select()` with a 200 ms timeout. On each timeout
expiry, the registered timer callbacks are fired in registration order. This
replaces `g_timeout_add(200, …)` / `XtAppAddTimeOut`.

### SIGWINCH

`SIGWINCH` is caught via `sigaction`. The handler writes one byte to a separate
`resizePipe_` fd. The main `select()` loop reads it and sets `resizePending_ =
true`, which triggers `handleResize()` before the next `redraw()`.

---

## 8. Startup & Shutdown Sequence

### Startup

```
main()
├── NcursesApp app(argc, argv)    // initscr(), colors, self-pipe
├── MainWindow win(app)           // domain objects, layout, load presets
└── app.run()                     // select() loop
```

### Shutdown

1. `Q` / `F10` → `app_.quit()` sets `quitRequested_ = true`.
2. `run()` exits its loop; `MainWindow` destructor runs:
   - Removes timer callback.
   - Stops recording if active.
   - Calls `audio_->stop()`.
   - Calls `settings_.save()`.
   - Destroys all `unique_ptr` members in reverse declaration order.
3. `NcursesApp` destructor: closes self-pipe, calls `endwin()`.

---

## 9. Color & Styling

```cpp
// Color pair indices
enum ColorPair : int {
    CP_NORMAL    = 1,   // white on dark grey (default text)
    CP_TITLE     = 2,   // bright white on blue (header bar)
    CP_FOCUSED   = 3,   // black on cyan (focused widget highlight)
    CP_ACTIVE    = 4,   // bright white on green (power ON / rec ON)
    CP_DISABLED  = 5,   // dark grey on dark grey (disabled widget)
    CP_BORDER    = 6,   // grey on dark grey (slot borders)
    CP_SLOT_HDR  = 7,   // yellow on dark grey (slot header row)
    CP_HINT      = 8,   // dark grey on black (key-hint bar)
    CP_BYPASS    = 9,   // bright red on dark grey (bypassed indicator)
    CP_DIALOG_BG = 10,  // white on dark blue (dialog background)
};
```

Initialised in `NcursesApp` constructor:

```cpp
init_pair(CP_NORMAL,    COLOR_WHITE,   COLOR_BLACK);
init_pair(CP_TITLE,     COLOR_WHITE,   COLOR_BLUE);
init_pair(CP_FOCUSED,   COLOR_BLACK,   COLOR_CYAN);
init_pair(CP_ACTIVE,    COLOR_WHITE,   COLOR_GREEN);
init_pair(CP_DISABLED,  COLOR_BLACK+8, COLOR_BLACK);  // dim
init_pair(CP_BORDER,    COLOR_WHITE,   COLOR_BLACK);
init_pair(CP_SLOT_HDR,  COLOR_YELLOW,  COLOR_BLACK);
init_pair(CP_HINT,      COLOR_BLACK+8, COLOR_BLACK);
init_pair(CP_BYPASS,    COLOR_RED,     COLOR_BLACK);
init_pair(CP_DIALOG_BG, COLOR_WHITE,   COLOR_BLUE);
```

Terminals that do not support colour fall back gracefully via
`has_colors()` check; in that case `A_REVERSE` is used for focus and `A_BOLD`
for active state.

---

## 10. Keyboard Reference

```
 Global
 ──────────────────────────────────────────────────────────────
  Q / F10          Quit
  F1 / ?           Show this help screen
  F2 / S           Open Settings dialog
  P                Toggle Power (engine on/off)
  R                Toggle Record
  1 / 2 / 3 / 4   Jump focus to plugin slot N
  Tab              Next focusable item
  Shift+Tab        Previous focusable item

 When a Slider is focused
 ──────────────────────────────────────────────────────────────
  ← / →            Adjust by one step
  PgUp / PgDn      Adjust by 10 steps
  Home / End       Jump to minimum / maximum

 When a DropdownList is open
 ──────────────────────────────────────────────────────────────
  ↑ / ↓            Navigate entries
  Enter            Select entry and close
  Esc              Close without change

 When a TextEntry is active
 ──────────────────────────────────────────────────────────────
  (printable)      Insert character
  Backspace        Delete previous character
  Enter            Commit text
  Esc              Discard changes

 In Plugin Browser / Settings Dialog
 ──────────────────────────────────────────────────────────────
  Tab              Next widget in dialog
  Shift+Tab        Previous widget
  Esc              Close without action
  Enter            Activate focused widget
```

---

## 11. Build Integration

### Directory

New sources live in `src/ncurses/`. Shared domain sources in `src/` are
compiled once (object files reused via CMake `OBJECT` library).

### CMakeLists.txt additions

```cmake
# Find ncurses with panel and menu sub-libraries
find_package(Curses REQUIRED)
# ncurses panels library
find_library(PANEL_LIB panel REQUIRED)

set(NCURSES_SOURCES
    src/ncurses/NcursesApp.cpp
    src/ncurses/MainWindow.cpp
    src/ncurses/ControlBar.cpp
    src/ncurses/PresetBar.cpp
    src/ncurses/PluginSlot.cpp
    src/ncurses/ParameterPanel.cpp
    src/ncurses/PluginDialog.cpp
    src/ncurses/SettingsDialog.cpp
    src/ncurses/main_ncurses.cpp
)

add_executable(opiqo-ncurses
    ${DOMAIN_SOURCES}      # shared object library target
    ${NCURSES_SOURCES}
)

target_link_libraries(opiqo-ncurses
    PRIVATE
    ${CURSES_LIBRARIES}
    ${PANEL_LIB}
    ${JACK_LIBRARIES}
    ${LILV_LIBRARIES}
    pthread
)

target_compile_definitions(opiqo-ncurses PRIVATE OPIQO_NCURSES=1)
```

### CMakePresets addition

```json
{
  "name": "linux-ncurses-debug",
  "displayName": "Linux ncurses Debug",
  "inherits": "linux-debug",
  "binaryDir": "${sourceDir}/build-linux-ncurses-debug",
  "cacheVariables": {
    "BUILD_NCURSES": "ON"
  }
}
```

### main entry point

`src/ncurses/main_ncurses.cpp`:

```cpp
#include "NcursesApp.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    NcursesApp app(argc, argv);
    MainWindow win(app);      // win holds reference to app
    app.run();
    return 0;
}
```

---

## 12. Porting Differences from Xlib

| Concern | Xlib/Xaw | ncurses |
|---|---|---|
| **Event loop** | `XtAppMainLoop()` / `XtAppNextEvent()` | `select()` on stdin + self-pipe with 200 ms timeout |
| **Cross-thread wakeup** | Write to `pipeFd_[1]`; `XtAppAddInput` reads it | Write to `pipeFd_[1]`; `select()` unblocks on `pipeFd_[0]` |
| **Timer** | `XtAppAddTimeOut(200)` registered once, re-registers itself | `select()` timeout IS the timer; callbacks in NcursesApp |
| **SIGWINCH / resize** | `XtAddEventHandler(StructureNotifyMask)` | `sigaction(SIGWINCH)` + resize pipe |
| **Widget toolkit** | Xt + Xaw (`XtCreateManagedWidget`) | Hand-drawn using `WINDOW*`, `waddstr`, `wattr*` |
| **Modal dialogs** | `XtPopup(XtGrabExclusive)` + separate `XtAppNextEvent` drain | `PANEL*` overlay + nested `select()` loop in `runModal()` |
| **File browser** | `XmFileSelectionDialog` equivalent via Xaw dialog | `promptFilePath()` text-entry overlay; no file-tree browser |
| **Port/preset dropdowns** | `SimpleMenu` popup widgets | `DropdownList` widget (`WINDOW*` overlay) |
| **Color/style** | `XAllocNamedColor` + `XChangeGC` | `init_pair()` / `COLOR_PAIR(n)` / `A_BOLD` / `A_REVERSE` |
| **Scrolling parameters** | `Viewport` widget | `scrollOffset_` counter + clipped `WINDOW*` draw |
| **Text input** | `AsciiText` Xaw widget | `TextEntry` widget backed by `std::string` + `wmove`/`waddch` |
| **Cursor visibility** | N/A (mouse pointer) | `curs_set(1)` when a `TextEntry` is editing; `curs_set(0)` otherwise |
| **Shared domain objects** | `AppSettings`, `AudioEngine`, etc. — compiled from `src/xlib/` copies | Headers identical; `AudioEngine.cpp` / `JackPortEnum.cpp` compiled from `src/xlib/` or symlinked |

### Key simplification opportunities vs. Xlib

- No render overhead. Drawing is text-only; a full redraw is cheap (< 1 ms).
- No separate `XlibApp` initialisation of display/visual/colors. `NcursesApp`
  is simpler.
- No Xt resource files, fallback fonts, or pixmap allocations.
- `ParameterPanel` does not need heap-allocated `Widget*` trees — each row is
  painted directly from the `PortDef` list and the rendering state held in
  `controlDataList_`.
- File path input replaces asynchronous `GtkFileDialog` / `XmFileSelection`
  with a synchronous `TextEntry` overlay; simpler but requires the user to
  type paths manually.

---

*End of implementation plan.*
