# Opiqo — Xlib Implementation Plan

> Based on: `design/gtk4.md` (canonical reference), `docs/Plan-gtk2.md` (GTK2 port)  
> Target toolkit: Xlib (libX11) + Xaw (Athena Widgets, libXaw)  
> Domain layer (`LiveEffectEngine`, `AudioEngine`, `FileWriter`, `LV2Plugin`, `AppSettings`, `JackPortEnum`) is **shared verbatim** with all other builds.

---

## Table of Contents

1. [Scope & Goals](#1-scope--goals)
2. [Toolkit Selection Rationale](#2-toolkit-selection-rationale)
3. [Source Layout](#3-source-layout)
4. [API Mapping: GTK4 → Xlib/Xaw](#4-api-mapping-gtk4--xlibxaw)
   - 4.1 [Window & Container Hierarchy](#41-window--container-hierarchy)
   - 4.2 [Buttons & Toggles](#42-buttons--toggles)
   - 4.3 [Sliders (GtkScale)](#43-sliders-gtkscale)
   - 4.4 [Dropdowns (GtkDropDown / GtkComboBoxText)](#44-dropdowns-gtkdropdown--gtkcomboboxtext)
   - 4.5 [Text Entry (GtkEntry)](#45-text-entry-gtkentry)
   - 4.6 [Labels](#46-labels)
   - 4.7 [Separators & Borders](#47-separators--borders)
   - 4.8 [Scrolled Areas (GtkScrolledWindow)](#48-scrolled-areas-gtkscrolledwindow)
   - 4.9 [List Widgets (GtkListBox / GtkTreeView)](#49-list-widgets-gtklistbox--gtktreeview)
   - 4.10 [Tabs (GtkNotebook)](#410-tabs-gtknotebook)
   - 4.11 [Dialogs (GtkWindow modal)](#411-dialogs-gtkwindow-modal)
   - 4.12 [File Chooser Dialog](#412-file-chooser-dialog)
   - 4.13 [About Dialog](#413-about-dialog)
   - 4.14 [Menu Bar](#414-menu-bar)
   - 4.15 [Styling (GtkCssProvider / GtkRC)](#415-styling-gtkcssprovider--gtkrc)
5. [Implementation Tasks](#5-implementation-tasks)
   - 5.1 [Build System](#51-build-system)
   - 5.2 [XlibApp — Application Wrapper](#52-xlibapp--application-wrapper)
   - 5.3 [main_xlib.cpp](#53-main_xlibcpp)
   - 5.4 [MainWindow](#54-mainwindow)
   - 5.5 [ControlBar](#55-controlbar)
   - 5.6 [PresetBar](#56-presetbar)
   - 5.7 [PluginSlot](#57-pluginslot)
   - 5.8 [ParameterPanel](#58-parameterpanel)
   - 5.9 [PluginDialog](#59-plugindialog)
   - 5.10 [SettingsDialog](#510-settingsdialog)
6. [Event Loop Architecture](#6-event-loop-architecture)
7. [Threading Model](#7-threading-model)
8. [Styling & Drawing](#8-styling--drawing)
9. [Timer Polling (poll engine state)](#9-timer-polling-poll-engine-state)
10. [Testing Checklist](#10-testing-checklist)
11. [Pitfall Reference Table](#11-pitfall-reference-table)

---

## 1. Scope & Goals

Produce a functional equivalent of the GTK4 Opiqo UI using pure Xlib (libX11) for window management and event handling, augmented by the Xaw (Athena Widgets) library for standard widget types. **No GTK** and **no Qt** dependency is introduced.

The user-visible feature set is identical to all other frontends:
- 2×2 grid of plugin slots with Add / Bypass / Remove controls and scrollable parameter panels.
- Bottom control bar: Power toggle, Gain slider, Format/Quality selectors, Record toggle, xrun counter, status label.
- Preset bar: named preset dropdown, name entry, Load / Save / Delete buttons.
- Settings dialog: JACK port selection, audio info, plugin cache invalidation, preset export/import.
- About dialog.
- Plugin browser dialog with live text-filter search.

The domain layer (`LiveEffectEngine`, `AudioEngine`, `FileWriter`, `LV2Plugin`, `AppSettings`, `JackPortEnum`) is **completely unchanged**.

---

## 2. Toolkit Selection Rationale

Three approaches were considered:

| Option | Pros | Cons |
|---|---|---|
| **A: Pure Xlib only** | Zero toolkit dependencies beyond libX11 | Must implement every widget from scratch: buttons, scrollbars, text input, dropdowns |
| **B: Xlib + Xaw (chosen)** | Provides ready-made widgets (Command, Toggle, Scrollbar, List, AsciiText, Form, Simple) | Xaw widgets look dated; layout with `Form` / `Box` is verbose; no built-in combo-box or tab widget |
| **C: Xlib + libXft + custom layer** | Better font rendering (anti-aliased) | Requires full custom widget system implementation |

**Chosen approach: Xlib + Xaw.**

Xaw (libXaw3d or libXaw) ships with every standard Linux/X11 installation. It provides the minimal set of widgets needed to avoid implementing text input, scroll, and list selection from scratch. The missing pieces (dropdown/popup, tab strip) are built as lightweight custom Xlib windows on top of Xaw.

### Required libraries

| Library | pkg-config name | Purpose |
|---|---|---|
| `libX11` | `x11` | Core X11 protocol, windows, events, GC |
| `libXaw` | `xaw7` | Athena widget toolkit (or `xaw3d` for 3D look) |
| `libXt` | `xt` | Xt Intrinsics (required by Xaw) |
| `libXmu` | `xmu` | Miscellaneous Xt utilities (needed by Xaw) |
| `libXext` | `xext` | X11 extensions (double-buffering via DBE) |
| `libXrender` | `xrender` | Optional: alpha-blended drawing if needed |

---

## 3. Source Layout

Create `src/xlib/` mirroring the structure of `src/gtk3/` and `src/gtk2/`. Files that contain no toolkit calls are symlinked from the GTK4 source.

```
src/xlib/
    AppSettings.h/.cpp        ← symlink from gtk4/ (unchanged)
    AudioEngine.h/.cpp        ← symlink from gtk4/ (unchanged)
    JackPortEnum.h/.cpp       ← symlink from gtk4/ (unchanged)
    version.h                 ← symlink from gtk4/ (unchanged)
    XlibApp.h/.cpp            ← new: X11 application init/event loop
    MainWindow.h/.cpp         ← port
    ControlBar.h/.cpp         ← port
    PresetBar.h/.cpp          ← port
    PluginSlot.h/.cpp         ← port
    ParameterPanel.h/.cpp     ← port
    PluginDialog.h/.cpp       ← custom list window
    SettingsDialog.h/.cpp     ← port
    main_xlib.cpp             ← entry point
```

The four symlinked files (`AppSettings`, `AudioEngine`, `JackPortEnum`, `version.h`) contain zero X11 calls and compile identically under any target.

---

## 4. API Mapping: GTK4 → Xlib/Xaw

This section documents the direct mapping from GTK4 concepts to Xlib/Xaw equivalents. The GTK ownership/lifecycle model maps to a C++ wrapper object per widget.

### 4.1 Window & Container Hierarchy

GTK uses a widget-tree with automatic layout. Xlib/Xaw uses explicit window creation with `XCreateWindow` / `XtCreateManagedWidget`. Layout is either coordinate-based (`XCreateWindow` with absolute geometry) or constraint-based (`XawForm` / `XawPaned`).

| GTK4 construct | Xlib/Xaw equivalent | Notes |
|---|---|---|
| `GtkApplicationWindow` | `XtAppInitialize` + `XtVaCreateManagedWidget("toplevel", applicationShellWidgetClass, ...)` | Top-level shell |
| `GtkWindow` (secondary) | `XtVaCreatePopupShell("name", transientShellWidgetClass, parent, ...)` | For dialogs |
| `GtkBox` (vertical) | `XtVaCreateManagedWidget("box", boxWidgetClass, parent, XtNorientation, XtVertical, ...)` | `XawBox` |
| `GtkBox` (horizontal) | `XtVaCreateManagedWidget("box", boxWidgetClass, parent, XtNorientation, XtHorizontal, ...)` | `XawBox` |
| `GtkGrid` (2×2) | `XtVaCreateManagedWidget("form", formWidgetClass, parent, ...)` + constraints | `XawForm` with position constraints; or use a `Box` with fixed sizes |
| `GtkFrame` | `XtVaCreateManagedWidget("frame", simpleWidgetClass, ...)` with border | Use a `Simple` widget with borderWidth, or a nested `Form` |
| `GtkScrolledWindow` | `XtVaCreateManagedWidget("vport", viewportWidgetClass, parent, ...)` | `XawViewport` with `XtNallowVert, True` |
| `GtkSeparator` | `XtVaCreateManagedWidget("sep", labelWidgetClass, parent, XtNlabel, "", XtNwidth, 1, XtNborderWidth, 1, ...)` | 1-pixel wide/tall label used as visual divider |
| `GtkHeaderBar` | `XawForm` row + `XawLabel` for title | Xaw has no header bar; use a form strip at the top |
| `GtkNotebook` (tabs) | Custom `XawSimple` strip + `XawForm` page area | See §4.10 |

**Xaw layout strategy for the main window:**

The root widget is an `XawPaned` (vertical panes):
1. Pane 1: `XawForm` — menu bar (File/View menu strip with `XawMenuButton` widgets).
2. Pane 2: `XawForm` — 2×2 slot grid (four `PluginSlot` frames arranged with `fromHoriz`/`fromVert` constraints).
3. Pane 3: `XawForm` — preset bar.
4. Pane 4: `XawForm` — control bar.

The `XawPaned` widget provides resizable panes. Disable resize handles by setting `XtNshowGrip, False` and `XtNresizeToPreferred, True` on non-resizable panes.

---

### 4.2 Buttons & Toggles

| GTK4 widget | Xaw equivalent | Xaw class name | Notes |
|---|---|---|---|
| `GtkButton` | `XawCommand` | `commandWidgetClass` | `XtNcallback` for click |
| `GtkToggleButton` | `XawToggle` | `toggleWidgetClass` | `XtNstate` for on/off; `XtNcallback` fires on change |
| `GtkCheckButton` | `XawToggle` | `toggleWidgetClass` | Same as toggle; rendered as checkbox by setting `XtNshapeStyle, XmuShapeRectangle` |

**Creating a command button:**

```cpp
Widget btn = XtVaCreateManagedWidget("Add",
    commandWidgetClass, parent,
    XtNlabel, "+ Add",
    nullptr);
XtAddCallback(btn, XtNcallback, addCallback, (XtPointer)this);
```

**Creating a toggle button:**

```cpp
Widget toggle = XtVaCreateManagedWidget("Power",
    toggleWidgetClass, parent,
    XtNlabel, "Power",
    XtNstate, False,
    nullptr);
XtAddCallback(toggle, XtNcallback, powerCallback, (XtPointer)this);
```

**Reading toggle state:**

```cpp
Boolean state = False;
XtVaGetValues(powerToggle_, XtNstate, &state, nullptr);
bool on = (state == True);
```

**Setting toggle state without firing callback:**

```cpp
XtRemoveAllCallbacks(powerToggle_, XtNcallback);
XtVaSetValues(powerToggle_, XtNstate, (Boolean)on, nullptr);
XtAddCallback(powerToggle_, XtNcallback, powerCallback, (XtPointer)this);
```

Alternatively, set `suppressSignals_` in the callback handler as done in the GTK builds.

---

### 4.3 Sliders (GtkScale)

| GTK4 | Xaw equivalent | Notes |
|---|---|---|
| `GtkScale` horizontal | `XawScrollbar` with `XtNorientation, XtHorizontal` | `XtNscrollProc` and `XtNjumpProc` callbacks |
| `GtkScale` vertical | `XawScrollbar` with `XtNorientation, XtVertical` | |
| `gtk_range_get_value` | Manual: maintain a `double currentVal_` member | `XawScrollbar` reports proportional position `[0.0, 1.0]`; map to `[min, max]` |
| `gtk_range_set_value` | `XawScrollbarSetThumb(sb, position, shown)` | `position` = `(val - min) / (max - min)` |

**Scrollbar as slider — key mappings:**

`XawScrollbar` is designed for scrolling, not for linear parameter adjustment. The `XtNscrollProc` callback fires on arrow clicks (increment/decrement) and the `XtNjumpProc` fires on thumb drag. Both must be handled:

```cpp
// jumpProc: thumb drag (proportional position [0..1])
static void onSliderJump(Widget w, XtPointer client, XtPointer call) {
    float pos = *(float*)call;          // position in [0.0, 1.0]
    SliderData* sd = (SliderData*)client;
    double val = sd->min + pos * (sd->max - sd->min);
    sd->self->onSliderChanged(sd->portIndex, val);
}

// scrollProc: arrow clicks (pixels scrolled, + or -)
static void onSliderScroll(Widget w, XtPointer client, XtPointer call) {
    int delta = (int)(intptr_t)call;
    SliderData* sd = (SliderData*)client;
    double step = (sd->max - sd->min) / 200.0;
    double val = std::clamp(sd->currentVal + (delta > 0 ? step : -step),
                            sd->min, sd->max);
    sd->currentVal = val;
    float pos = (val - sd->min) / (sd->max - sd->min);
    XawScrollbarSetThumb(w, pos, 0.05f);  // thumb size = 5%
    sd->self->onSliderChanged(sd->portIndex, val);
}
```

**Heap-allocated `SliderData` struct** (freed in `ParameterPanel::clear()`):

```cpp
struct SliderData {
    ParameterPanel* self;
    int    portIndex;
    double min, max, currentVal;
};
```

---

### 4.4 Dropdowns (GtkDropDown / GtkComboBoxText)

Xaw has no built-in combo-box or popup-menu widget suitable for a dropdown selector. The replacement is an `XawMenuButton` + `XawSimpleMenu` + `XawSmeBSB` (menu entries):

```
XawMenuButton ("WAV ▾")
    → popup: XawSimpleMenu
        XawSmeBSB "WAV"
        XawSmeBSB "MP3"
        XawSmeBSB "OGG"
        XawSmeBSB "OPUS"
        XawSmeBSB "FLAC"
```

**Construction:**

```cpp
// The menuName resource on XawMenuButton auto-connects to a SimpleMenu
// widget with the same name in the same parent shell.

Widget menuBtn = XtVaCreateManagedWidget("formatDrop",
    menuButtonWidgetClass, parent,
    XtNlabel, "WAV",
    XtNmenuName, "formatMenu",
    nullptr);

Widget menu = XtVaCreatePopupShell("formatMenu",
    simpleMenuWidgetClass, menuBtn,
    nullptr);

const char* labels[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
for (int i = 0; i < 5; ++i) {
    char name[16]; snprintf(name, sizeof(name), "fmt%d", i);
    Widget item = XtVaCreateManagedWidget(name,
        smeBSBObjectClass, menu,
        XtNlabel, labels[i],
        nullptr);
    // Callback data must carry button widget + index:
    DropData* dd = new DropData{menuBtn, i, this};
    XtAddCallback(item, XtNcallback, onFormatSelected, (XtPointer)dd);
}
```

**Callback — update button label to reflect selection:**

```cpp
struct DropData { Widget button; int index; void* self; };

static void onFormatSelected(Widget, XtPointer client, XtPointer) {
    DropData* dd = (DropData*)client;
    const char* labels[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
    XtVaSetValues(dd->button, XtNlabel, labels[dd->index], nullptr);
    ((ControlBar*)dd->self)->onFormatChanged(dd->index);
}
```

**Reading current selection:**

Maintain `int currentFormat_` in `ControlBar`. Updated in every `onFormatSelected` call. No direct API to "query" an `XawMenuButton`.

**Preset dropdown (named presets, large list):**

For the preset list which may have many entries and needs to show the currently-selected name, use the same `XawMenuButton` + `XawSimpleMenu` pattern. Rebuild the popup menu whenever `setPresetNames()` is called by destroying old menu children and creating new `SmeBSB` items.

---

### 4.5 Text Entry (GtkEntry)

| GTK4 | Xaw equivalent | Notes |
|---|---|---|
| `GtkEntry` | `XawAsciiText` in `XawtextEditMultiClick` mode | `XtNstring`, `XtNeditType, XawtextEdit` |
| `gtk_entry_get_text` | `XtVaGetValues(w, XtNstring, &str, ...)` | `str` is the internal buffer; copy with `strdup` |
| `gtk_entry_set_text` | `XtVaSetValues(w, XtNstring, text, ...)` | |
| `GtkSearchEntry` | Plain `XawAsciiText` + `XtNcallback` chain | Use `XtAddEventHandler` for `KeyPressMask` to fire filter |

**Single-line editable text field:**

```cpp
Widget entry = XtVaCreateManagedWidget("nameEntry",
    asciiTextWidgetClass, parent,
    XtNwidth, 140,
    XtNheight, 22,
    XtNeditType, XawtextEdit,
    XtNstring, "",
    XtNdisplayCaret, True,
    nullptr);
```

**Reading the text value:**

```cpp
String val = nullptr;
XtVaGetValues(entry, XtNstring, &val, nullptr);
std::string result = val ? val : "";
// Do NOT free 'val'; it points to the widget's internal buffer.
```

**Live search filter (PluginDialog search box):**

Install an `XtAddEventHandler` for `KeyPressMask` on the `AsciiText` widget. After processing the key event (letting Xaw handle it first via `XtDispatchEvent`), read the updated string and call `rebuildList()`.

```cpp
XtAddEventHandler(searchWidget_, KeyPressMask, False,
    +[](Widget w, XtPointer client, XEvent*, Boolean*) {
        // Defer to next iteration so XawAsciiText processes the key first:
        XtAppAddWorkProc(appContext_,
            +[](XtPointer cd) -> Boolean {
                PluginDialog* self = (PluginDialog*)cd;
                String val = nullptr;
                XtVaGetValues(self->searchWidget_, XtNstring, &val, nullptr);
                self->rebuildList(val ? val : "");
                return True;  // remove work proc after one call
            }, client);
    }, (XtPointer)this, False);
```

---

### 4.6 Labels

| GTK4 | Xaw equivalent | Notes |
|---|---|---|
| `GtkLabel` | `XawLabel` | `XtNlabel` resource |
| `gtk_label_set_text(lbl, text)` | `XtVaSetValues(lbl, XtNlabel, text, nullptr)` | |
| `gtk_label_set_xalign(lbl, 0.0)` | `XtVaSetValues(lbl, XtNjustify, XtJustifyLeft, nullptr)` | `XtJustifyLeft` / `XtJustifyRight` / `XtJustifyCenter` |
| `gtk_label_set_ellipsize` | Not directly supported | Truncate string manually if needed |
| Bold / colour label | `XtNfont` / `XtNforeground` | Load `XLoadQueryFont` or use `XftFont` |

**Setting the status label text at runtime:**

```cpp
XtVaSetValues(statusLabel_, XtNlabel, msg.c_str(), nullptr);
```

> **Note:** `XtNlabel` takes a `char*`, not a `const char*`. Safe as long as the widget makes its own copy (Xaw does). The passed pointer need not persist after the call.

---

### 4.7 Separators & Borders

Xaw has no dedicated separator widget. Options:

| Approach | Implementation |
|---|---|
| Horizontal line | `XawLabel` with `XtNwidth` = parent width, `XtNheight = 2`, `XtNlabel = ""`, `XtNborderWidth = 1` |
| Vertical line | `XawLabel` with `XtNwidth = 2`, `XtNheight` = parent height, `XtNlabel = ""`, `XtNborderWidth = 1` |
| Frame-like border | Set `XtNborderWidth` on any Xaw widget or `XCreateWindow` |
| `GtkFrame` equivalent | `XawForm` with `XtNdefaultDistance = 4` containing child widgets |

---

### 4.8 Scrolled Areas (GtkScrolledWindow)

| GTK4 | Xaw equivalent | Notes |
|---|---|---|
| `GtkScrolledWindow` (vertical auto) | `XawViewport` with `XtNallowVert, True` | Child content sits inside the viewport |
| `gtk_container_add(sw, box)` | `XtVaCreateManagedWidget("box", boxWidgetClass, viewport, ...)` | Child is created with the viewport as parent |
| Scroll policy `NEVER` (horizontal) | `XtNallowHoriz, False` | Default |
| Scroll policy `AUTOMATIC` (vertical) | `XtNallowVert, True`, `XtNuseBottom, True` | Scrollbar appears on right |

**Creating a scrolled parameter panel:**

```cpp
scroll_ = XtVaCreateManagedWidget("scroll",
    viewportWidgetClass, parent,
    XtNallowVert, True,
    XtNallowHoriz, False,
    XtNuseBottom, True,
    XtNwidth, 200,
    XtNheight, 180,
    nullptr);

box_ = XtVaCreateManagedWidget("paramBox",
    boxWidgetClass, scroll_,   // viewport is the parent
    XtNorientation, XtVertical,
    XtNhSpace, 2,
    XtNvSpace, 2,
    nullptr);
```

---

### 4.9 List Widgets (GtkListBox / GtkTreeView)

Xaw provides `XawList` (a simple single-column list widget) which is sufficient for the plugin browser's flat list of plugin names.

| GTK4 / GTK3 | Xaw equivalent | Notes |
|---|---|---|
| `GtkListBox` / `GtkTreeView` + `GtkListStore` | `XawList` | Single-column, `XtNlist` takes `String*` array |
| Add / update entries | Call `XawListChange(list, strings, count, longest, resizable)` | Replaces entire list atomically |
| Get selection | `XawListReturnStruct* info = XawListShowCurrent(list)` | Returns index and label |
| Callback on selection | `XtNcallback` (fires on single click) | Or `XtNdefaultAction` (fires on double click / Return) |

**Plugin list construction in PluginDialog:**

```cpp
// Initial empty list:
static String emptyList[] = {nullptr};
listWidget_ = XtVaCreateManagedWidget("pluginList",
    listWidgetClass, scroll_,
    XtNlist, emptyList,
    XtNdefaultColumns, 1,
    XtNforceColumns, True,
    XtNverticalList, True,
    nullptr);

XtAddCallback(listWidget_, XtNcallback, onSelectionChanged, (XtPointer)this);
XtAddCallback(listWidget_, XtNdefaultAction, onDoubleClick, (XtPointer)this);
```

**Rebuilding the list after a filter change:**

```cpp
void PluginDialog::rebuildList(const std::string& filter) {
    filtered_.clear();
    // Build filtered_ vector of PluginEntry ...

    // Build String* array (Xaw requires char**, not const char**)
    listStrings_.resize(filtered_.size() + 1);
    for (size_t i = 0; i < filtered_.size(); ++i)
        listStrings_[i] = const_cast<char*>(filtered_[i].displayName.c_str());
    listStrings_.back() = nullptr;

    int longest = 0;
    for (auto& s : filtered_)
        longest = std::max(longest, (int)s.displayName.size());

    XawListChange(listWidget_,
        listStrings_.data(), (int)filtered_.size(),
        longest * 8,   // approx pixel width of longest string
        True);         // resizable
}
```

> **Note:** `XawListChange` does **not** copy the strings. The `listStrings_` vector and `filtered_` entries must outlive the list widget. Clear them only after calling `XawListChange` with new content.

---

### 4.10 Tabs (GtkNotebook)

Xaw has no tab widget. Implement a minimal tab strip:

1. Create an `XawForm` to hold the tab strip (a row of `XawToggle` buttons acting as tab selectors).
2. Below it, show only the currently active `XawForm` page; hide others with `XtUnmanageChild`.

```cpp
// Tab toggle group — use XawToggle radioGroup
Widget audioTab = XtVaCreateManagedWidget("Audio",
    toggleWidgetClass, tabStrip_,
    XtNstate, True,
    XtNradioGroup, nullptr,  // first in group
    nullptr);
Widget presetTab = XtVaCreateManagedWidget("Presets",
    toggleWidgetClass, tabStrip_,
    XtNradioGroup, audioTab,  // same radio group
    nullptr);

XtAddCallback(audioTab,  XtNcallback, onTabChanged, (XtPointer)this);
XtAddCallback(presetTab, XtNcallback, onTabChanged, (XtPointer)this);
```

**Switching pages:**

```cpp
void SettingsDialog::onTabChanged(int index) {
    XtUnmanageChild(audioPage_);
    XtUnmanageChild(presetPage_);
    if (index == 0) XtManageChild(audioPage_);
    else            XtManageChild(presetPage_);
}
```

---

### 4.11 Dialogs (GtkWindow modal)

| GTK4 / GTK2 | Xlib/Xaw equivalent | Notes |
|---|---|---|
| `GtkWindow` (modal, transient) | `XtVaCreatePopupShell("dlg", transientShellWidgetClass, parent, ...)` | `XtNoverrideRedirect, False` for WM-decorated |
| `gtk_window_set_modal` | `XSetWMHints(dpy, win, &hints)` with `InputHint` | Set via Motif hints or just grab pointer: `XtGrabKeyboard` + `XtGrabPointer` |
| Show | `XtPopup(shell, XtGrabNonexclusive)` | `XtGrabExclusive` for fully modal |
| Hide (persistent dialog) | `XtPopdown(shell)` | Equivalent to `gtk_widget_hide` |
| Destroy | `XtDestroyWidget(shell)` | Equivalent to `gtk_window_destroy` |

**Modal popup flow:**

```cpp
// Show modal dialog:
XtPopup(dialogShell_, XtGrabExclusive);

// In confirm/cancel callback:
XtPopdown(dialogShell_);
XtDestroyWidget(dialogShell_);  // or just XtPopdown for persistent dialogs
```

---

### 4.12 File Chooser Dialog

Xaw has no file chooser. The simplest approach for the Xlib port is a custom dialog with a text-entry field asking the user to type a file path. A full GTK-style file browser widget is out of scope.

**Minimal file-path input dialog:**

```cpp
class SimpleFileDialog {
    // Transient popup shell with:
    //   XawLabel: "Enter file path:"
    //   XawAsciiText: path input
    //   XawCommand: "OK"
    //   XawCommand: "Cancel"
public:
    // Runs modally. Returns "" on cancel.
    std::string run(const std::string& title, bool forSave);
};
```

**`run()` implementation using a nested event loop:**

```cpp
std::string SimpleFileDialog::run(const std::string& title, bool forSave) {
    buildWidgets(title, forSave);
    XtPopup(shell_, XtGrabExclusive);

    // Nested event loop — blocking until done_ is set:
    done_ = false;
    while (!done_) {
        XEvent evt;
        XtAppNextEvent(appContext_, &evt);
        XtDispatchEvent(&evt);
    }

    XtPopdown(shell_);
    XtDestroyWidget(shell_);
    return result_;
}
```

The `done_` flag is set by the OK/Cancel button callbacks; `result_` holds the text from the `AsciiText` path entry.

> **Usability note:** For the export/import preset path, this is sufficient. Production-quality file navigation (directory browsing) can be deferred to a future enhancement.

---

### 4.13 About Dialog

A simple transient popup shell containing `XawLabel` widgets for program name, version, codename, and authors, plus an `XawCommand` "OK" button.

```cpp
void MainWindow::showAboutDialog() {
    Widget shell = XtVaCreatePopupShell("About Opiqo",
        transientShellWidgetClass, topLevel_,
        XtNwidth, 320, XtNheight, 200,
        nullptr);
    Widget form = XtVaCreateManagedWidget("form",
        formWidgetClass, shell, nullptr);

    auto makeLabel = [&](const char* name, const char* text, Widget from) {
        return XtVaCreateManagedWidget(name, labelWidgetClass, form,
            XtNlabel, text,
            XtNfromVert, from,
            XtNjustify, XtJustifyCenter,
            XtNborderWidth, 0,
            nullptr);
    };

    Widget l1 = makeLabel("title", "Opiqo",               nullptr);
    Widget l2 = makeLabel("ver",   "Version " APP_VERSION, l1);
    Widget l3 = makeLabel("code",  "\"" APP_CODENAME "\"", l2);
    Widget l4 = makeLabel("auth",  "Opiqo contributors",  l3);

    Widget ok = XtVaCreateManagedWidget("OK",
        commandWidgetClass, form,
        XtNfromVert, l4,
        nullptr);

    XtAddCallback(ok, XtNcallback,
        +[](Widget, XtPointer sh, XtPointer) {
            XtPopdown((Widget)sh);
            XtDestroyWidget((Widget)sh);
        }, (XtPointer)shell);

    XtPopup(shell, XtGrabNonexclusive);
    XRaiseWindow(XtDisplay(shell), XtWindow(shell));
}
```

---

### 4.14 Menu Bar

| GTK4 / GTK2 | Xaw equivalent | Notes |
|---|---|---|
| `GtkMenuBar` | `XawSimpleMenu` inside `XawMenuButton` per top-level menu | |
| `GtkMenuItem` | `XawSmeBSB` (with label) | Clickable menu entry |
| `GtkSeparatorMenuItem` | `XawSmeLine` | Horizontal rule in menu |
| `GtkMenuBar/MenuShell` | `XawBox` (horizontal) of `XawMenuButton` widgets | Each top-level menu is a `MenuButton` |

**File menu + View menu construction:**

```cpp
// Horizontal box acting as menu bar:
Widget menuBar = XtVaCreateManagedWidget("menuBar",
    boxWidgetClass, rootPane_,
    XtNorientation, XtHorizontal,
    XtNhSpace, 0,
    nullptr);

// "File" menu:
Widget fileBtn = XtVaCreateManagedWidget("File",
    menuButtonWidgetClass, menuBar,
    XtNmenuName, "fileMenu",
    nullptr);
Widget fileMenu = XtVaCreatePopupShell("fileMenu",
    simpleMenuWidgetClass, fileBtn, nullptr);
Widget quitItem = XtVaCreateManagedWidget("Quit",
    smeBSBObjectClass, fileMenu,
    XtNlabel, "Quit", nullptr);
XtAddCallback(quitItem, XtNcallback,
    +[](Widget, XtPointer self, XtPointer) {
        ((MainWindow*)self)->quit();
    }, (XtPointer)this);

// "View" menu:
Widget viewBtn = XtVaCreateManagedWidget("View",
    menuButtonWidgetClass, menuBar,
    XtNmenuName, "viewMenu",
    nullptr);
Widget viewMenu = XtVaCreatePopupShell("viewMenu",
    simpleMenuWidgetClass, viewBtn, nullptr);

Widget settingsItem = XtVaCreateManagedWidget("Settings",
    smeBSBObjectClass, viewMenu,
    XtNlabel, "Settings…", nullptr);
Widget sepItem = XtVaCreateManagedWidget("sep",
    smeLineObjectClass, viewMenu, nullptr);
Widget aboutItem = XtVaCreateManagedWidget("About",
    smeBSBObjectClass, viewMenu,
    XtNlabel, "About", nullptr);

XtAddCallback(settingsItem, XtNcallback,
    +[](Widget, XtPointer s, XtPointer) { ((MainWindow*)s)->openSettings(); },
    (XtPointer)this);
XtAddCallback(aboutItem, XtNcallback,
    +[](Widget, XtPointer s, XtPointer) { ((MainWindow*)s)->showAboutDialog(); },
    (XtPointer)this);
```

---

### 4.15 Styling (GtkCssProvider / GtkRC)

Xaw widgets are styled via X resource database (Xrm) entries or via direct `XtVaSetValues` calls at creation time. There is no CSS or RC equivalent.

**Colour / font strategy:**

| GTK concept | Xlib/Xaw equivalent |
|---|---|
| CSS `background-color` | `XtNbackground` / `XtNforeground` on widget |
| CSS red button (destructive) | `XtNbackground, red_pixel` on the `XawCommand` |
| CSS blue button (suggested-action) | `XtNbackground, blue_pixel` |
| CSS bold font | `XtNfont, boldFont` (load with `XLoadQueryFont`) |
| CSS `color` (text) | `XtNforeground, pixel` |

**Allocating named colours:**

```cpp
// In XlibApp::init():
Display* dpy = XtDisplay(topLevel_);
Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));

XColor red, blue, grey;
XAllocNamedColor(dpy, cmap, "red3",       &red,  &red);
XAllocNamedColor(dpy, cmap, "steelblue3", &blue, &blue);
XAllocNamedColor(dpy, cmap, "grey70",     &grey, &grey);

// Store pixels globally for widget creation:
colors_.red  = red.pixel;
colors_.blue = blue.pixel;
colors_.grey = grey.pixel;
```

**Applying colour to a button:**

```cpp
XtVaSetValues(deleteButton_,
    XtNbackground, appColors_.red,
    XtNforeground, WhitePixelOfScreen(XtScreen(deleteButton_)),
    nullptr);
```

**Loading fonts:**

```cpp
// Default font (monospace fallback):
XFontStruct* font = XLoadQueryFont(dpy, "fixed");
// Bold for plugin name labels:
XFontStruct* boldFont = XLoadQueryFont(dpy,
    "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-iso8859-*");
if (!boldFont) boldFont = font;  // graceful fallback
```

**Application-wide X resources (`.Xresources` or `XrmPutStringResource`):**

Optionally ship a default resource string and merge it into the resource database at startup:

```cpp
const char* kDefaultResources[] = {
    "opiqo*font:            -*-helvetica-medium-r-*-*-12-*-*-*-*-*-iso8859-*",
    "opiqo*background:      #2b2b2b",
    "opiqo*foreground:      #e0e0e0",
    "opiqo*Command.background: #3a3a3a",
    "opiqo*Toggle.background:  #3a3a3a",
    nullptr
};
XrmDatabase rdb = XtDatabase(XtDisplay(topLevel_));
for (const char** r = kDefaultResources; *r; ++r)
    XrmPutLineResource(&rdb, *r);
```

---

## 5. Implementation Tasks

### 5.1 Build System

**In `CMakeLists.txt`**, add a new block (after the existing `linux-gtk2` block):

```cmake
if(OPIQO_TARGET_PLATFORM STREQUAL "linux-xlib")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11_LIBS REQUIRED IMPORTED_TARGET
        x11 xt xmu xaw7 xext)

    set(XLIB_SRCS
        src/xlib/main_xlib.cpp
        src/xlib/XlibApp.cpp
        src/xlib/MainWindow.cpp
        src/xlib/ControlBar.cpp
        src/xlib/PresetBar.cpp
        src/xlib/PluginSlot.cpp
        src/xlib/ParameterPanel.cpp
        src/xlib/PluginDialog.cpp
        src/xlib/SettingsDialog.cpp
        src/xlib/AppSettings.cpp
        src/xlib/AudioEngine.cpp
        src/xlib/JackPortEnum.cpp
        # shared domain sources:
        src/LiveEffectEngine.cpp
        src/FileWriter.cpp
        src/LockFreeQueue.cpp
    )

    add_executable(opiqo-xlib ${XLIB_SRCS})
    target_include_directories(opiqo-xlib PRIVATE src src/xlib
        ${X11_LIBS_INCLUDE_DIRS})
    target_compile_options(opiqo-xlib PRIVATE
        ${X11_LIBS_CFLAGS_OTHER}
        -DOPIQO_XLIB)
    target_link_libraries(opiqo-xlib
        PkgConfig::X11_LIBS
        jack lilv-0 sndfile opus opusenc mp3lame FLAC
        -ldl -lpthread)
endif()
```

**In `CMakePresets.json`**, add:

```json
{
    "name": "linux-xlib",
    "displayName": "Linux Xlib/Xaw (JACK)",
    "binaryDir": "${sourceDir}/build-linux-xlib",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "OPIQO_TARGET_PLATFORM": "linux-xlib"
    }
},
{
    "name": "linux-xlib-debug",
    "displayName": "Linux Xlib/Xaw Debug (JACK)",
    "binaryDir": "${sourceDir}/build-linux-xlib-debug",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "OPIQO_TARGET_PLATFORM": "linux-xlib"
    }
}
```

**Build presets (also in `CMakePresets.json`):**

```json
{ "name": "linux-xlib",       "configurePreset": "linux-xlib" },
{ "name": "linux-xlib-debug", "configurePreset": "linux-xlib-debug" }
```

**In `Makefile`** (convenience targets):

```makefile
xlib:
	cmake --preset linux-xlib && cmake --build build-linux-xlib --parallel

xlib-debug:
	cmake --preset linux-xlib-debug && cmake --build build-linux-xlib-debug --parallel
```

---

### 5.2 XlibApp — Application Wrapper

`XlibApp` encapsulates the Xt application context, display, and top-level shell. It is the Xlib equivalent of `GtkApplication`.

**`src/xlib/XlibApp.h`:**

```cpp
#pragma once
#include <X11/Intrinsic.h>
#include <cstdint>

struct AppColors {
    Pixel red, blue, grey, white, black;
};

class XlibApp {
public:
    XlibApp(int& argc, char** argv);
    ~XlibApp();

    XtAppContext appContext() const { return appCtx_; }
    Widget       topLevel()   const { return topLevel_; }
    Display*     display()    const { return XtDisplay(topLevel_); }
    const AppColors& colors() const { return colors_; }

    // Run the event loop. Returns when the top-level shell is destroyed.
    void run();

    // Request quit (sets quit_ flag; processed in run()).
    void quit();

    // Install a 200 ms timer, replacing g_timeout_add(200, ...).
    // Returns a handle that can be passed to removeTimer().
    XtIntervalId addTimer(unsigned long ms,
                          XtTimerCallbackProc proc, XtPointer data);
    void removeTimer(XtIntervalId id);

    // Self-pipe: used by AudioEngine to wake the Xt event loop.
    // Returns read-fd (registered as Xt input source).
    int  selfPipeReadFd()  const { return pipeFd_[0]; }
    int  selfPipeWriteFd() const { return pipeFd_[1]; }

private:
    XtAppContext appCtx_ = nullptr;
    Widget       topLevel_ = nullptr;
    AppColors    colors_ {};
    bool         quit_ = false;
    int          pipeFd_[2] = {-1, -1};

    void initColors();
};
```

**`src/xlib/XlibApp.cpp` — key methods:**

```cpp
XlibApp::XlibApp(int& argc, char** argv) {
    static XrmOptionDescRec options[] = {};
    topLevel_ = XtVaAppInitialize(&appCtx_,
        "Opiqo", options, 0, &argc, argv, nullptr,
        XtNtitle, "Opiqo",
        XtNwidth,  960,
        XtNheight, 680,
        nullptr);
    initColors();

    // Create self-pipe for cross-thread event wakeup:
    pipe(pipeFd_);
    // Register read end as Xt input source:
    XtAppAddInput(appCtx_, pipeFd_[0],
        (XtPointer)(intptr_t)XtInputReadMask,
        +[](XtPointer data, int* /*fd*/, XtInputId*) {
            // Drain pipe; actual work queued via XtAppAddWorkProc
            char buf[64]; read(((XlibApp*)data)->pipeFd_[0], buf, sizeof(buf));
        }, (XtPointer)this);
}

void XlibApp::run() {
    XtRealizeWidget(topLevel_);
    XtAppMainLoop(appCtx_);
}

void XlibApp::quit() {
    XtAppSetExitFlag(appCtx_);
}

XtIntervalId XlibApp::addTimer(unsigned long ms,
                                XtTimerCallbackProc proc, XtPointer data) {
    return XtAppAddTimeOut(appCtx_, ms, proc, data);
}
```

---

### 5.3 main_xlib.cpp

```cpp
// src/xlib/main_xlib.cpp
#include "XlibApp.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    XlibApp app(argc, argv);
    MainWindow win(app);
    win.realize();
    app.run();
    return 0;
}
```

`win.realize()` calls `XtRealizeWidget` on all managed children (equivalent to `gtk_widget_show_all`).

---

### 5.4 MainWindow

**`MainWindow.h` — class skeleton:**

```cpp
#pragma once
#include <X11/Intrinsic.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "../LiveEffectEngine.h"
#include "../FileWriter.h"
#include "XlibApp.h"
#include "AppSettings.h"
#include "AudioEngine.h"
#include "JackPortEnum.h"

class ControlBar;
class PresetBar;
class PluginSlot;
class SettingsDialog;

class MainWindow {
public:
    explicit MainWindow(XlibApp& app);
    ~MainWindow();

    void realize();

private:
    // Domain layer:
    std::unique_ptr<LiveEffectEngine> engine_;
    AppSettings                       settings_;
    std::unique_ptr<AudioEngine>      audio_;
    std::unique_ptr<JackPortEnum>     portEnum_;

    // Frontend:
    XlibApp&                          app_;
    Widget                            topLevel_;   // alias to app.topLevel()
    Widget                            rootPane_;
    PluginSlot*                       slots_[4] = {};
    std::unique_ptr<ControlBar>       controlBar_;
    std::unique_ptr<PresetBar>        presetBar_;
    std::unique_ptr<SettingsDialog>   settingsDlg_;

    // State:
    bool                              isRecording_ = false;
    XtIntervalId                      timerId_     = 0;
    std::vector<nlohmann::json>       namedPresets_;

    void buildWidgets();
    void buildMenuBar(Widget parent);
    void buildSlotGrid(Widget parent);

    // Callbacks (equivalent to GTK handlers):
    void onPowerToggled(bool on);
    void onGainChanged(float gain);
    void onRecordToggled(bool start, int format, int quality);
    void onAddPlugin(int slot);
    void onDeletePlugin(int slot);
    void onBypassPlugin(int slot, bool bypassed);
    void onSetValue(int slot, uint32_t portIndex, float value);
    void onSetFilePath(int slot, const std::string& uri, const std::string& path);
    void openSettings();
    void onSettingsApply(AppSettings s);
    void showAboutDialog();
    void onPresetLoad();
    void onPresetSave(const std::string& name);
    void onPresetDelete();
    void loadNamedPresets();
    void saveNamedPresets();
    void applyFullPreset(const nlohmann::json& data);
    void setStatus(const std::string& msg);
    void quit();

    // Preset I/O:
    std::string onExportPreset();
    void        onImportPreset(const std::string& path);

    // Plugin cache:
    void savePluginCache();
    bool loadPluginCache();

    // Timer callback (replaces g_timeout_add):
    static void pollEngineState(XtPointer self, XtIntervalId*);
};
```

**Constructor:**

```cpp
MainWindow::MainWindow(XlibApp& app) : app_(app), topLevel_(app.topLevel()) {
    // Domain layer (identical to GTK builds):
    engine_   = std::make_unique<LiveEffectEngine>();
    settings_ = AppSettings::load();
    portEnum_ = std::make_unique<JackPortEnum>();
    audio_    = std::make_unique<AudioEngine>(engine_.get());

    if (!loadPluginCache()) {
        engine_->initPlugins();
        savePluginCache();
    }

    // JACK hot-plug callback (marshal to Xt via self-pipe):
    portEnum_->setChangeCallback([this]() {
        char byte = 1;
        write(app_.selfPipeWriteFd(), &byte, 1);
        XtAppAddWorkProc(app_.appContext(),
            +[](XtPointer d) -> Boolean {
                ((MainWindow*)d)->onPortsChanged();
                return True;
            }, this);
    });

    // Audio error callback:
    audio_->setErrorCallback([this](const std::string& msg) {
        // Must be called on the Xt main thread via work proc
        // (AudioEngine already marshals via g_idle_add equivalent)
        setStatus("Audio error: " + msg);
        controlBar_->setPowerState(false);
    });

    buildWidgets();

    // Install 200 ms timer (replaces g_timeout_add(200, ...)):
    timerId_ = XtAppAddTimeOut(app_.appContext(), 200,
        pollEngineState, (XtPointer)this);
}
```

**`buildWidgets()`:**

```cpp
void MainWindow::buildWidgets() {
    // Root: vertical paned widget
    rootPane_ = XtVaCreateManagedWidget("rootPane",
        panedWidgetClass, topLevel_,
        nullptr);

    buildMenuBar(rootPane_);

    buildSlotGrid(rootPane_);

    // Horizontal separator pane:
    XtVaCreateManagedWidget("sep",
        labelWidgetClass, rootPane_,
        XtNlabel, "",
        XtNheight, 2,
        XtNshowGrip, False,
        nullptr);

    // PresetBar and ControlBar (each creates its own pane child):
    presetBar_  = std::make_unique<PresetBar>(rootPane_, app_);
    controlBar_ = std::make_unique<ControlBar>(rootPane_, app_);

    // Wire callbacks (identical logic to GTK builds):
    controlBar_->setPowerCb([this](bool on) { onPowerToggled(on); });
    controlBar_->setGainCb([this](float g)  { onGainChanged(g); });
    controlBar_->setRecordCb([this](bool s, int f, int q) {
        onRecordToggled(s, f, q);
    });

    for (int i = 0; i < 4; ++i) {
        slots_[i] = new PluginSlot(i + 1, app_);
        slots_[i]->setAddCb([this](int s)        { onAddPlugin(s); });
        slots_[i]->setDeleteCb([this](int s)      { onDeletePlugin(s); });
        slots_[i]->setBypassCb([this](int s, bool b) { onBypassPlugin(s, b); });
        slots_[i]->setValueCb([this](int s, uint32_t p, float v) {
            onSetValue(s, p, v);
        });
        slots_[i]->setFileCb([this](int s, const std::string& u, const std::string& p) {
            onSetFilePath(s, u, p);
        });
    }

    presetBar_->setLoadCb([this]() { onPresetLoad(); });
    presetBar_->setSaveCb([this](const std::string& n) { onPresetSave(n); });
    presetBar_->setDeleteCb([this]() { onPresetDelete(); });

    loadNamedPresets();
}
```

**`buildSlotGrid(Widget parent)`:**

Xaw `Form` constraints are used to place four slot frames in a 2×2 grid:

```cpp
void MainWindow::buildSlotGrid(Widget parent) {
    Widget gridForm = XtVaCreateManagedWidget("slotGrid",
        formWidgetClass, parent,
        XtNdefaultDistance, 4,
        nullptr);

    // Attach each PluginSlot frame to the form using fromHoriz/fromVert:
    // Slot 1: top-left
    Widget w0 = slots_[0]->frame();
    XtVaSetValues(w0,
        XtNtop,    XtChainTop,
        XtNleft,   XtChainLeft,
        XtNright,  XtRubber,
        XtNbottom, XtRubber,
        nullptr);
    XtManageChild(w0);

    // Slot 2: top-right, fromHoriz = slot 1
    Widget w1 = slots_[1]->frame();
    XtVaSetValues(w1,
        XtNfromHoriz, w0,
        XtNtop,       XtChainTop,
        XtNright,     XtChainRight,
        XtNbottom,    XtRubber,
        nullptr);
    XtManageChild(w1);

    // Slot 3: bottom-left, fromVert = slot 1
    Widget w2 = slots_[2]->frame();
    XtVaSetValues(w2,
        XtNfromVert, w0,
        XtNleft,     XtChainLeft,
        XtNright,    XtRubber,
        XtNbottom,   XtChainBottom,
        nullptr);
    XtManageChild(w2);

    // Slot 4: bottom-right, fromHoriz = slot 3, fromVert = slot 2
    Widget w3 = slots_[3]->frame();
    XtVaSetValues(w3,
        XtNfromHoriz, w2,
        XtNfromVert,  w1,
        XtNright,     XtChainRight,
        XtNbottom,    XtChainBottom,
        nullptr);
    XtManageChild(w3);
}
```

**`pollEngineState` (static timer callback):**

```cpp
void MainWindow::pollEngineState(XtPointer client, XtIntervalId*) {
    MainWindow* self = (MainWindow*)client;

    // Check for error state (same logic as GTK pollEngineState):
    if (self->audio_->state() == AudioEngine::State::Error) {
        self->controlBar_->setPowerState(false);
        if (self->isRecording_) {
            self->isRecording_ = false;
            self->engine_->stopRecording();
            self->controlBar_->setRecordingActive(false);
        }
    }
    // Update xrun counter:
    self->controlBar_->setXrunCount(self->audio_->xrunCount());

    // Reschedule (replaces G_SOURCE_CONTINUE):
    self->timerId_ = XtAppAddTimeOut(self->app_.appContext(), 200,
        pollEngineState, client);
}
```

**Destructor:**

```cpp
MainWindow::~MainWindow() {
    if (timerId_) XtRemoveTimeOut(timerId_);
    if (isRecording_) engine_->stopRecording();
    audio_->stop();
    settings_.save();
    for (int i = 0; i < 4; ++i) delete slots_[i];
}
```

---

### 5.5 ControlBar

The `ControlBar` widget lives in a horizontal `XawForm` pane. The constructor takes `Widget parent` (the paned container) and `XlibApp& app`.

**Member widgets:**

| Member | Xaw type | Resource |
|---|---|---|
| `powerToggle_` | `XawToggle` | `XtNlabel = "Power"` |
| `gainSlider_` | `XawScrollbar` horizontal | mapped `[0.0, 2.0]` → `[0.0, 1.0]` |
| `formatDrop_` | `XawMenuButton` + `XawSimpleMenu` | `XtNmenuName = "formatMenu"` |
| `qualityDrop_` | `XawMenuButton` + `XawSimpleMenu` | Hidden when lossless format selected |
| `qualityPane_` | `XawForm` housing quality label + drop | Managed/unmanaged to show/hide |
| `recordToggle_` | `XawToggle` | `XtNlabel = "● Record"` |
| `xrunLabel_` | `XawLabel` | `XtNlabel = "Xruns: 0"` |
| `statusLabel_` | `XawLabel` | `XtNlabel = "Stopped"` |

**Format change → hide/show quality pane:**

```cpp
void ControlBar::onFormatChanged(int fmt) {
    if (suppressSignals_) return;
    currentFormat_ = fmt;
    bool lossy = (fmt == 1 || fmt == 2 || fmt == 3);
    if (lossy)
        XtManageChild(qualityPane_);
    else
        XtUnmanageChild(qualityPane_);

    if (formatCb_) formatCb_(fmt);
}
```

**`setPowerState` — without firing callback:**

```cpp
void ControlBar::setPowerState(bool on) {
    suppressSignals_ = true;
    XtVaSetValues(powerToggle_, XtNstate, (Boolean)on, nullptr);
    suppressSignals_ = false;
}
```

**`setStatusText`:**

```cpp
void ControlBar::setStatusText(const std::string& msg) {
    XtVaSetValues(statusLabel_, XtNlabel, msg.c_str(), nullptr);
}
```

**`setXrunCount`:**

```cpp
void ControlBar::setXrunCount(uint64_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Xruns: %" PRIu64, n);
    XtVaSetValues(xrunLabel_, XtNlabel, buf, nullptr);
}
```

---

### 5.6 PresetBar

The `PresetBar` lives in a horizontal `XawForm` pane.

**Member widgets:**

| Member | Xaw type | Notes |
|---|---|---|
| `presetDrop_` | `XawMenuButton` + `XawSimpleMenu` | Rebuilt by `setPresetNames()` |
| `nameEntry_` | `XawAsciiText` | Editable; width = 140 |
| `loadBtn_`, `saveBtn_`, `deleteBtn_` | `XawCommand` | Standard buttons |

**`setPresetNames()`:**

Destroy all `SmeBSB` children of the preset popup and recreate them.

```cpp
void PresetBar::setPresetNames(const std::vector<std::string>& names) {
    // Destroy old menu popup and recreate it:
    if (presetMenu_) {
        XtDestroyWidget(presetMenu_);
        presetMenu_ = nullptr;
    }

    presetMenu_ = XtVaCreatePopupShell("presetMenu",
        simpleMenuWidgetClass, presetDrop_, nullptr);

    presetNames_ = names;  // store copy for index queries
    int idx = 0;
    for (const auto& name : names) {
        char wname[32]; snprintf(wname, sizeof(wname), "ps%d", idx);
        Widget item = XtVaCreateManagedWidget(wname,
            smeBSBObjectClass, presetMenu_,
            XtNlabel, name.c_str(), nullptr);

        struct ItemData { PresetBar* bar; int index; };
        ItemData* id = new ItemData{this, idx};
        XtAddCallback(item, XtNcallback,
            +[](Widget, XtPointer cd, XtPointer) {
                auto* d = (ItemData*)cd;
                d->bar->onPresetSelected(d->index);
                delete d;
            }, (XtPointer)id);
        ++idx;
    }

    // Update button label:
    if (!names.empty())
        XtVaSetValues(presetDrop_, XtNlabel, names[0].c_str(), nullptr);
    else
        XtVaSetValues(presetDrop_, XtNlabel, "(none)", nullptr);
}
```

**`onPresetSelected(int idx)`:**

```cpp
void PresetBar::onPresetSelected(int idx) {
    selectedIndex_ = idx;
    if (idx >= 0 && idx < (int)presetNames_.size()) {
        XtVaSetValues(presetDrop_, XtNlabel, presetNames_[idx].c_str(), nullptr);
        XtVaSetValues(nameEntry_,  XtNstring, presetNames_[idx].c_str(), nullptr);
    }
}
```

**`getCurrentName()`:**

```cpp
std::string PresetBar::getCurrentName() const {
    String val = nullptr;
    XtVaGetValues(nameEntry_, XtNstring, &val, nullptr);
    return val ? std::string(val) : "";
}
```

---

### 5.7 PluginSlot

Each `PluginSlot` creates a bordered `XawForm` (acting as the frame), a header `XawBox`, and a parameter area `XawViewport`.

**`buildWidgets()`:**

```cpp
void PluginSlot::buildWidgets() {
    char title[32]; snprintf(title, sizeof(title), "Slot %d", slot_);

    // Frame = top-level Form with border
    frame_ = XtVaCreateWidget("slotFrame",
        formWidgetClass, parent_,
        XtNborderWidth, 1,
        nullptr);

    // Header box:
    Widget headerBox = XtVaCreateManagedWidget("header",
        boxWidgetClass, frame_,
        XtNorientation, XtHorizontal,
        XtNhSpace, 2,
        nullptr);

    nameLabel_ = XtVaCreateManagedWidget("slotName",
        labelWidgetClass, headerBox,
        XtNlabel, title,
        XtNjustify, XtJustifyLeft,
        XtNborderWidth, 0,
        nullptr);

    addButton_ = XtVaCreateManagedWidget("addBtn",
        commandWidgetClass, headerBox,
        XtNlabel, "+ Add", nullptr);

    bypassButton_ = XtVaCreateManagedWidget("bypassBtn",
        toggleWidgetClass, headerBox,
        XtNlabel, "Bypass",
        XtNsensitive, False,
        nullptr);

    deleteButton_ = XtVaCreateManagedWidget("deleteBtn",
        commandWidgetClass, headerBox,
        XtNlabel, "× Remove",
        XtNsensitive, False,
        XtNbackground, app_.colors().red,
        XtNforeground, WhitePixelOfScreen(XtScreen(deleteButton_)),
        nullptr);

    // Separator:
    XtVaCreateManagedWidget("slotSep",
        labelWidgetClass, frame_,
        XtNlabel, "",
        XtNfromVert, headerBox,
        XtNheight, 2,
        XtNwidth, 200,
        XtNborderWidth, 1,
        nullptr);

    // Parameter panel viewport (rest of the frame):
    Widget paramWidget = paramPanel_->viewport();
    XtVaSetValues(paramWidget, XtNfromVert, /* sep widget */, nullptr);
    XtManageChild(paramWidget);

    // Wire callbacks:
    XtAddCallback(addButton_, XtNcallback,
        +[](Widget, XtPointer s, XtPointer) {
            auto* self = (PluginSlot*)s;
            if (self->addCb_) self->addCb_(self->slot_);
        }, (XtPointer)this);

    XtAddCallback(bypassButton_, XtNcallback,
        +[](Widget, XtPointer s, XtPointer) {
            auto* self = (PluginSlot*)s;
            Boolean state = False;
            XtVaGetValues(self->bypassButton_, XtNstate, &state, nullptr);
            if (self->bypassCb_) self->bypassCb_(self->slot_, (bool)state);
        }, (XtPointer)this);

    XtAddCallback(deleteButton_, XtNcallback,
        +[](Widget, XtPointer s, XtPointer) {
            auto* self = (PluginSlot*)s;
            if (self->deleteCb_) self->deleteCb_(self->slot_);
        }, (XtPointer)this);
}
```

**`onPluginAdded` / `onPluginCleared`:**

```cpp
void PluginSlot::onPluginAdded(const std::string& name,
                                const std::vector<LV2Plugin::PortInfo>& ports) {
    XtVaSetValues(nameLabel_,     XtNlabel, name.c_str(), nullptr);
    XtVaSetValues(bypassButton_,  XtNsensitive, True,     nullptr);
    XtVaSetValues(deleteButton_,  XtNsensitive, True,     nullptr);
    XtVaSetValues(bypassButton_,  XtNstate, False,        nullptr);
    paramPanel_->build(ports);
}

void PluginSlot::onPluginCleared() {
    char title[32]; snprintf(title, sizeof(title), "Slot %d", slot_);
    XtVaSetValues(nameLabel_,    XtNlabel, title,  nullptr);
    XtVaSetValues(bypassButton_, XtNsensitive, False, nullptr);
    XtVaSetValues(deleteButton_, XtNsensitive, False, nullptr);
    XtVaSetValues(bypassButton_, XtNstate, False,     nullptr);
    paramPanel_->clear();
}
```

---

### 5.8 ParameterPanel

`ParameterPanel` manages a `XawViewport` (`scroll_`) containing a `XawBox` (`box_`). Parameter widgets are created inside `box_` and destroyed via `XtDestroyWidget`.

**Constructor:**

```cpp
ParameterPanel::ParameterPanel(Widget parent, XlibApp& app)
    : app_(app) {
    scroll_ = XtVaCreateWidget("paramScroll",
        viewportWidgetClass, parent,
        XtNallowVert,  True,
        XtNallowHoriz, False,
        XtNheight, 180,
        nullptr);

    box_ = XtVaCreateManagedWidget("paramBox",
        boxWidgetClass, scroll_,
        XtNorientation, XtVertical,
        XtNvSpace, 2,
        nullptr);
}
```

**`build(const std::vector<LV2Plugin::PortInfo>& ports)`:**

```cpp
void ParameterPanel::build(const std::vector<LV2Plugin::PortInfo>& ports) {
    clear();

    for (const auto& port : ports) {
        // Row: horizontal box
        Widget row = XtVaCreateManagedWidget("row",
            boxWidgetClass, box_,
            XtNorientation, XtHorizontal,
            XtNhSpace, 4,
            nullptr);

        // Label:
        XtVaCreateManagedWidget("lbl",
            labelWidgetClass, row,
            XtNlabel, port.label.c_str(),
            XtNwidth, 150,
            XtNjustify, XtJustifyLeft,
            XtNborderWidth, 0,
            nullptr);

        // Control widget based on port type:
        Widget ctrl = nullptr;
        switch (port.type) {
        case LV2Plugin::ControlType::Toggle: {
            ctrl = XtVaCreateManagedWidget("toggle",
                toggleWidgetClass, row,
                XtNstate, (port.defaultVal > 0.5f) ? True : False,
                XtNlabel, "",
                nullptr);
            auto* cd = new ControlData{this, port.index};
            controlDataList_.push_back(cd);
            XtAddCallback(ctrl, XtNcallback,
                +[](Widget w, XtPointer cd_, XtPointer) {
                    auto* cd = (ControlData*)cd_;
                    Boolean state = False;
                    XtVaGetValues(w, XtNstate, &state, nullptr);
                    if (cd->panel->valueCb_)
                        cd->panel->valueCb_(cd->portIndex, state ? 1.0f : 0.0f);
                }, cd);
            break;
        }
        case LV2Plugin::ControlType::Trigger: {
            ctrl = XtVaCreateManagedWidget("fire",
                commandWidgetClass, row,
                XtNlabel, "Fire",
                nullptr);
            auto* cd = new ControlData{this, port.index};
            controlDataList_.push_back(cd);
            XtAddCallback(ctrl, XtNcallback,
                +[](Widget, XtPointer cd_, XtPointer) {
                    auto* cd = (ControlData*)cd_;
                    if (cd->panel->valueCb_)
                        cd->panel->valueCb_(cd->portIndex, 1.0f);
                }, cd);
            break;
        }
        case LV2Plugin::ControlType::AtomFilePath: {
            ctrl = XtVaCreateManagedWidget("browse",
                commandWidgetClass, row,
                XtNlabel, "Browse…",
                nullptr);
            struct BrowseCd { ParameterPanel* panel; std::string uri; };
            auto* bcd = new BrowseCd{this, port.uri};
            controlDataList_.push_back(reinterpret_cast<ControlData*>(bcd));
            XtAddCallback(ctrl, XtNcallback,
                +[](Widget, XtPointer cd_, XtPointer) {
                    auto* bcd = (BrowseCd*)cd_;
                    SimpleFileDialog dlg(bcd->panel->app_);
                    std::string path = dlg.run("Open File", false);
                    if (!path.empty() && bcd->panel->fileCb_)
                        bcd->panel->fileCb_(bcd->uri, path);
                }, bcd);
            break;
        }
        case LV2Plugin::ControlType::Float:
        default: {
            if (port.isEnum) {
                // Use MenuButton dropdown for enum:
                ctrl = buildEnumControl(row, port);
            } else {
                // Scrollbar as slider:
                ctrl = XtVaCreateManagedWidget("slider",
                    scrollbarWidgetClass, row,
                    XtNorientation, XtHorizontal,
                    XtNwidth, 160,
                    nullptr);
                float pos = (port.max > port.min)
                    ? (port.defaultVal - port.min) / (port.max - port.min)
                    : 0.0f;
                XawScrollbarSetThumb(ctrl, pos, 0.05f);

                auto* sd = new SliderData{this, port.index,
                    port.min, port.max, port.defaultVal};
                controlDataList_.push_back(reinterpret_cast<ControlData*>(sd));
                XtAddCallback(ctrl, XtNjumpProc,
                    +[](Widget w, XtPointer cd_, XtPointer pos_) {
                        auto* sd = (SliderData*)cd_;
                        float pos = *(float*)pos_;
                        double val = sd->min + pos * (sd->max - sd->min);
                        sd->currentVal = val;
                        if (sd->panel->valueCb_)
                            sd->panel->valueCb_(sd->portIndex, (float)val);
                    }, sd);
                XtAddCallback(ctrl, XtNscrollProc,
                    +[](Widget w, XtPointer cd_, XtPointer delta_) {
                        auto* sd = (SliderData*)cd_;
                        int delta = (int)(intptr_t)delta_;
                        double step = (sd->max - sd->min) / 200.0;
                        double val = std::clamp(
                            sd->currentVal + (delta > 0 ? step : -step),
                            sd->min, sd->max);
                        sd->currentVal = val;
                        float pos = (sd->max > sd->min)
                            ? (float)((val - sd->min) / (sd->max - sd->min))
                            : 0.0f;
                        XawScrollbarSetThumb(w, pos, 0.05f);
                        if (sd->panel->valueCb_)
                            sd->panel->valueCb_(sd->portIndex, (float)val);
                    }, sd);
            }
            break;
        }
        }
    }
    XtManageChild(scroll_);
}
```

**`clear()`:**

```cpp
void ParameterPanel::clear() {
    // Destroy all children of box_:
    WidgetList children;
    Cardinal num = 0;
    XtVaGetValues(box_, XtNchildren, &children, XtNnumChildren, &num, nullptr);
    for (Cardinal i = 0; i < num; ++i)
        XtDestroyWidget(children[i]);

    // Free heap-allocated callback data:
    for (auto* cd : controlDataList_) ::operator delete(cd);
    controlDataList_.clear();
}
```

---

### 5.9 PluginDialog

The `PluginDialog` uses a transient popup shell containing a search `XawAsciiText`, an `XawViewport` + `XawList`, and Cancel / Add Plugin buttons.

**Key design:**

1. All `filtered_` entries have a `displayName` string = `"<name>  •  <author>"`.
2. The `XawList` shows `displayName` strings.
3. The URI corresponding to the selected index is retrieved from `filtered_[idx].uri`.

**`buildWidgets()`:**

```cpp
void PluginDialog::buildWidgets() {
    shell_ = XtVaCreatePopupShell("Add Plugin",
        transientShellWidgetClass, app_.topLevel(),
        XtNwidth, 540,
        XtNheight, 480,
        nullptr);

    Widget form = XtVaCreateManagedWidget("dlgForm",
        formWidgetClass, shell_,
        XtNdefaultDistance, 6,
        nullptr);

    // Search box:
    searchWidget_ = XtVaCreateManagedWidget("search",
        asciiTextWidgetClass, form,
        XtNwidth, 520,
        XtNheight, 22,
        XtNeditType, XawtextEdit,
        XtNstring, "",
        nullptr);

    // Live-update list on keystroke:
    XtAddEventHandler(searchWidget_, KeyPressMask, False,
        +[](Widget, XtPointer d, XEvent*, Boolean*) {
            auto* self = (PluginDialog*)d;
            XtAppAddWorkProc(self->app_.appContext(),
                +[](XtPointer cd) -> Boolean {
                    auto* pd = (PluginDialog*)cd;
                    String val = nullptr;
                    XtVaGetValues(pd->searchWidget_, XtNstring, &val, nullptr);
                    pd->rebuildList(val ? val : "");
                    return True;
                }, d);
        }, (XtPointer)this, False);

    // Viewport + list:
    Widget vp = XtVaCreateManagedWidget("listScroll",
        viewportWidgetClass, form,
        XtNallowVert, True,
        XtNfromVert, searchWidget_,
        XtNwidth, 520,
        XtNheight, 380,
        nullptr);

    static String emptyList[] = {nullptr};
    listWidget_ = XtVaCreateManagedWidget("pluginList",
        listWidgetClass, vp,
        XtNlist, emptyList,
        XtNdefaultColumns, 1,
        XtNforceColumns, True,
        XtNverticalList, True,
        nullptr);

    XtAddCallback(listWidget_, XtNdefaultAction,
        +[](Widget, XtPointer d, XtPointer) {
            ((PluginDialog*)d)->onConfirm();
        }, (XtPointer)this);

    // Button row:
    Widget btnBox = XtVaCreateManagedWidget("btnBox",
        boxWidgetClass, form,
        XtNorientation, XtHorizontal,
        XtNfromVert, vp,
        XtNhSpace, 8,
        nullptr);

    Widget cancelBtn = XtVaCreateManagedWidget("Cancel",
        commandWidgetClass, btnBox,
        XtNlabel, "Cancel", nullptr);
    confirmBtn_ = XtVaCreateManagedWidget("AddPlugin",
        commandWidgetClass, btnBox,
        XtNlabel, "Add Plugin",
        XtNbackground, app_.colors().blue,
        XtNforeground, WhitePixelOfScreen(XtScreen(confirmBtn_)),
        nullptr);

    XtAddCallback(cancelBtn, XtNcallback,
        +[](Widget, XtPointer d, XtPointer) {
            ((PluginDialog*)d)->onCancel();
        }, (XtPointer)this);
    XtAddCallback(confirmBtn_, XtNcallback,
        +[](Widget, XtPointer d, XtPointer) {
            ((PluginDialog*)d)->onConfirm();
        }, (XtPointer)this);

    rebuildList("");
    XtPopup(shell_, XtGrabNonexclusive);
}
```

**`onConfirm()`:**

```cpp
void PluginDialog::onConfirm() {
    XawListReturnStruct* info = XawListShowCurrent(listWidget_);
    if (!info || info->list_index == XAW_LIST_NONE) return;

    int idx = info->list_index;
    if (idx >= 0 && idx < (int)filtered_.size()) {
        if (confirmCb_) confirmCb_(filtered_[idx].uri);
    }
    XtPopdown(shell_);
    XtDestroyWidget(shell_);
    delete this;
}
```

**`onCancel()`:**

```cpp
void PluginDialog::onCancel() {
    XtPopdown(shell_);
    XtDestroyWidget(shell_);
    delete this;
}
```

---

### 5.10 SettingsDialog

The `SettingsDialog` is a persistent transient shell (shown/hidden with `XtPopup`/`XtPopdown`, not destroyed). It reuses the tab-strip pattern from §4.10.

**Construction:**

```cpp
SettingsDialog::SettingsDialog(Widget parent, XlibApp& app)
    : app_(app) {
    shell_ = XtVaCreatePopupShell("Settings",
        transientShellWidgetClass, parent,
        XtNwidth, 480,
        XtNheight, 360,
        nullptr);

    // Intercept WM close → just hide:
    Atom wmDelete = XInternAtom(XtDisplay(shell_), "WM_DELETE_WINDOW", False);
    XSetWMProtocols(XtDisplay(shell_), XtWindow(shell_), &wmDelete, 1);
    XtAddEventHandler(shell_, NoEventMask, True,
        +[](Widget w, XtPointer, XEvent* e, Boolean*) {
            if (e->type == ClientMessage) XtPopdown(w);
        }, nullptr);

    Widget outerForm = XtVaCreateManagedWidget("settingsForm",
        formWidgetClass, shell_, nullptr);

    // Tab strip:
    tabStrip_ = XtVaCreateManagedWidget("tabStrip",
        boxWidgetClass, outerForm,
        XtNorientation, XtHorizontal,
        nullptr);
    buildTabStrip();

    // Page area:
    Widget pageArea = XtVaCreateManagedWidget("pageArea",
        formWidgetClass, outerForm,
        XtNfromVert, tabStrip_,
        nullptr);

    buildAudioPage(pageArea);
    buildPresetsPage(pageArea);
    showPage(0);  // Show audio tab by default
}
```

**`buildAudioPage(Widget parent)`:**

Uses `XawForm` with `XtNfromVert` constraints to stack label/dropdown rows.

| Row | Left widget | Right widget |
|---|---|---|
| 0 | `XawLabel "Capture L:"` | `XawMenuButton` capLDrop_ |
| 1 | `XawLabel "Capture R:"` | `XawMenuButton` capRDrop_ |
| 2 | `XawLabel "Playback L:"` | `XawMenuButton` pbLDrop_ |
| 3 | `XawLabel "Playback R:"` | `XawMenuButton` pbRDrop_ |
| 4 | `XawLabel "Sample Rate:"` | `XawLabel` srLabel_ |
| 5 | `XawLabel "Block Size:"` | `XawLabel` bsLabel_ |
| 6 | `XawCommand "Apply"` | — |
| 7 | `XawCommand "Delete Plugin Cache"` | — |

Port dropdowns use the `XawMenuButton` + `XawSimpleMenu` pattern from §4.4.

**`populatePorts()`:**

Destroys and recreates each port menu's `SmeBSB` children with the new port list, then sets the active selection to match `AppSettings`.

**`onApply()`:**

```cpp
void SettingsDialog::onApply() {
    AppSettings s;
    s.capturePort  = selectedPortId(capLDrop_, capLPorts_);
    s.capturePort2 = selectedPortId(capRDrop_, capRPorts_);
    s.playbackPort = selectedPortId(pbLDrop_,  pbLPorts_);
    s.playbackPort2 = selectedPortId(pbRDrop_, pbRPorts_);
    if (applyCb_) applyCb_(s);
}
```

`selectedPortId()` maps the `currentCapL_` etc. integer index back to the corresponding `PortInfo::id` string.

**`show()` / `hide()`:**

```cpp
void SettingsDialog::show() {
    XtRealizeWidget(shell_);
    XtPopup(shell_, XtGrabNonexclusive);
    XRaiseWindow(XtDisplay(shell_), XtWindow(shell_));
}

void SettingsDialog::hide() {
    XtPopdown(shell_);
}
```

---

## 6. Event Loop Architecture

The Xlib port replaces GTK's `g_main_loop` with the Xt application event loop:

```
XtAppMainLoop(appCtx_)
    ↓ calls XtAppNextEvent() → XtDispatchEvent() → widget callbacks
    ↓ also dispatches:
        XtAppAddTimeOut  → pollEngineState every 200 ms
        XtAppAddInput    → self-pipe read (for JACK → UI thread marshalling)
        XtAppAddWorkProc → deferred work (search filter updates, etc.)
```

**Self-pipe pattern** (replaces `g_idle_add`):

JACK callbacks run on a real-time thread. To safely notify the UI thread:

1. JACK callback writes a single byte to `pipeFd_[1]` (the write end of the self-pipe).
2. Xt's `XtAppAddInput` on `pipeFd_[0]` wakes the event loop.
3. The input handler adds a `XtAppAddWorkProc` that executes on the main thread.

```
JACK RT thread                         Xt main thread
─────────────────────────────────────────────────────
write(pipeFd_[1], &byte, 1)
                                       XtAppAddInput fires
                                       read(pipeFd_[0], buf, sizeof(buf))
                                       XtAppAddWorkProc(errorWorkProc)
                                         → setStatus()
                                         → controlBar_->setPowerState(false)
```

---

## 7. Threading Model

| Thread | Runs | Xlib/Xt calls allowed |
|---|---|---|
| Xt main thread | All UI code, callbacks, timers, work procs | Yes (all Xt/Xlib calls) |
| JACK real-time thread | `jack_process_callback` inside `AudioEngine` | **No** — only write to self-pipe |

**Rules:**
- All `std::function` callbacks registered on domain objects fire on the Xt main thread.
- Cross-thread communication from JACK to Xt uses the self-pipe exclusively.
- Shared state between threads (`state_`, `sampleRate_`, `blockSize_`, `xrunCount_`) uses `std::atomic`.
- `g_idle_add` in `AudioEngine` must be replaced with `write(app_.selfPipeWriteFd(), ...)` for the Xlib build. This requires `AudioEngine` to accept a write-FD or a marshalling callback at construction:

```cpp
// In XlibApp AudioEngine wiring (MainWindow constructor):
audio_->setMarshalCallback([this](std::function<void()> fn) {
    // Store fn in a queue, write a byte to self-pipe
    {
        std::lock_guard<std::mutex> lk(marshalMutex_);
        marshalQueue_.push_back(std::move(fn));
    }
    char byte = 1;
    write(app_.selfPipeWriteFd(), &byte, 1);
});
```

The self-pipe input handler then drains `marshalQueue_` and calls each function on the main thread.

---

## 8. Styling & Drawing

There is no CSS or RC file. Visual styling is applied at widget creation time using Xt resources:

| Visual intent | Implementation |
|---|---|
| Dark background | Set `XtNbackground` on all widgets to `#2b2b2b` via `.Xresources` or `XrmPutLineResource` |
| White text | `XtNforeground = WhitePixelOfScreen(...)` |
| Red delete button | `XtNbackground = colors_.red` on `XawCommand` |
| Blue confirm button | `XtNbackground = colors_.blue` on `XawCommand` |
| Slot frame border | `XtNborderWidth = 1` on the frame `XawForm` |
| Bold plugin name | `XtNfont = boldFont_` loaded via `XLoadQueryFont` |
| Grey subtitle text | `XtNforeground = colors_.grey` on sub-label widgets |

**Loading fonts at startup (in `XlibApp::init()`):**

```cpp
Display* dpy = XtDisplay(topLevel_);
boldFont_  = XLoadQueryFont(dpy,
    "-*-helvetica-bold-r-normal-*-12-*-*-*-*-*-iso8859-1");
if (!boldFont_)
    boldFont_ = XLoadQueryFont(dpy, "fixed");
```

**XftFont (optional enhancement):**

For anti-aliased text, load `XftFont` and draw text manually with `XftDrawStringUtf8` on an off-screen pixmap, then composite onto the widget. This is only necessary if the Xaw default bitmap fonts are unacceptable. Defer to a post-MVP enhancement.

---

## 9. Timer Polling (poll engine state)

The GTK build uses `g_timeout_add(200, pollEngineState, this)`. The Xlib replacement uses `XtAppAddTimeOut`:

```cpp
// Initial installation (in MainWindow constructor):
timerId_ = XtAppAddTimeOut(app_.appContext(), 200,
    MainWindow::pollEngineState, (XtPointer)this);

// At the end of the callback (Xt timers are one-shot):
void MainWindow::pollEngineState(XtPointer client, XtIntervalId*) {
    MainWindow* self = (MainWindow*)client;
    // ... check state, update labels ...

    // Reschedule:
    self->timerId_ = XtAppAddTimeOut(self->app_.appContext(), 200,
        pollEngineState, client);
}
```

> **Key difference from GTK:** `XtAppAddTimeOut` callbacks are **one-shot** (unlike `g_timeout_add` which returns `G_SOURCE_CONTINUE` to reschedule). The callback must re-register itself each time it fires.

---

## 10. Testing Checklist

| Feature | What to verify |
|---|---|
| **Build** | `cmake --preset linux-xlib && cmake --build build-linux-xlib` completes without errors |
| **Launch** | `./build-linux-xlib/opiqo-xlib` opens a 960×680 window |
| **Plugin scan** | Plugin cache is written; subsequent launches skip rescan |
| **Power toggle** | JACK client connects; xrun counter starts incrementing |
| **Add plugin** | PluginDialog opens with full list; search filter works; double-click confirms |
| **Bypass toggle** | Engine reports plugin disabled; button insensitive when no plugin loaded |
| **Remove plugin** | Slot resets to "Slot N"; parameter panel cleared |
| **Gain slider** | Moving scrollbar-slider changes engine gain; engine gain re-applied on next start |
| **Format dropdown** | Selecting lossy format shows quality drop; lossless hides it |
| **Record toggle** | WAV file created in `~/Music`; file grows during recording; stops on second toggle |
| **Preset save** | Typed name appears in dropdown; JSON written to `~/.config/opiqo/opiqo_named_presets.json` |
| **Preset load** | All four slots restored with correct plugins and parameter values |
| **Preset delete** | Entry removed from dropdown; JSON updated |
| **Settings port selection** | Port dropdowns populated from JACK; Apply rebuilds AudioEngine with new ports |
| **Plugin cache delete** | Button removes cache file; next launch rescans |
| **Export preset** | File path dialog appears; JSON written to chosen path |
| **Import preset** | File path dialog appears; valid JSON restores all four slots |
| **About dialog** | Shows version, codename, authors; OK dismisses |
| **Engine error** | Power toggle resets to off when JACK disconnects |
| **Quit menu** | File → Quit destroys all windows cleanly; no Xt warnings in stderr |
| **Xruns** | xrun counter increments correctly when JACK reports xruns |

---

## 11. Pitfall Reference Table

| Area | GTK behaviour | Xlib/Xaw equivalent | Common mistake |
|---|---|---|---|
| Widget show/hide | `gtk_widget_set_visible` | `XtManageChild` / `XtUnmanageChild` | Using `XtMapWidget`/`XtUnmapWidget` instead (those map at X level but don't remove from layout) |
| Timer | `g_timeout_add` (repeating) | `XtAppAddTimeOut` (one-shot) | Forgetting to re-register at end of callback |
| Cross-thread callback | `g_idle_add` | self-pipe + `XtAppAddInput` + `XtAppAddWorkProc` | Direct Xlib calls from JACK RT thread (undefined behaviour) |
| Modal dialog | `gtk_window_set_modal` + `gtk_main` | `XtPopup(XtGrabExclusive)` | Using `XtGrabNone` (dialog non-modal) |
| Combo box | `GtkComboBoxText` / `GtkDropDown` | `XawMenuButton` + `XawSimpleMenu` | Reading "selected index" from widget (must maintain state manually) |
| Scrolled list | `GtkScrolledWindow` + `GtkListBox` | `XawViewport` + `XawList` | `XawListChange` with dangling `char*` pointers (strings must outlive list) |
| Text entry | `GtkEntry` | `XawAsciiText` with `XawtextEdit` | Calling `g_free` on the string returned by `XtVaGetValues(XtNstring)` (it is a pointer into the widget's buffer) |
| Signal suppression | `suppressSignals_` + `gtk_toggle_button_set_active` | Same flag + `XtVaSetValues(XtNstate)` | Removing and re-adding callbacks (laborious); flag pattern preferred |
| Nested event loop (file dialog) | `gtk_dialog_run()` | `XtAppNextEvent()` loop + `done_` flag | Calling `XtDestroyWidget` inside the nested loop before it exits |
| Menu button popup | Automatic via resource name | `XtNmenuName` must match the `XtVaCreatePopupShell` widget name exactly | Name mismatch → popup never appears (no error message from Xaw) |
| Tab navigation | `GtkNotebook` | `XawToggle` radio group + `XtManageChild`/`XtUnmanageChild` | Forgetting to unmanage the previously visible page before managing the new one |
| WM close button | `GtkApplication` handles automatically | Must register `WM_DELETE_WINDOW` Atom and handle `ClientMessage` event | Not handling → WM kills process without clean shutdown |
| Font loading | CSS `font-family` | `XLoadQueryFont` with XLFD pattern | `XLoadQueryFont` returns `nullptr` for unavailable font — always provide fallback |
| Colour allocation | CSS hex colour | `XAllocNamedColor` or `XAllocColor` | Not checking return value; no fallback if colourmap is full |
