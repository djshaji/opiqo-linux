# Opiqo — GTK2 Implementation Plan

> Based on: `design/gtk4.md` (canonical reference) and `docs/Plan-gtk3.md` (GTK3 port)  
> Target toolkit: GTK 2.x (minimum GTK 2.20; tested against GTK 2.24)  
> Domain layer (`LiveEffectEngine`, `AudioEngine`, `FileWriter`, `LV2Plugin`, `AppSettings`, `JackPortEnum`) is **shared verbatim** with the GTK4 build.

---

## Table of Contents

1. [Scope & Goals](#1-scope--goals)
2. [Source Layout](#2-source-layout)
3. [API Mapping: GTK4 → GTK2](#3-api-mapping-gtk4--gtk2)
   - 3.1 [Container / Layout Widgets](#31-container--layout-widgets)
   - 3.2 [Buttons & Input](#32-buttons--input)
   - 3.3 [Dropdowns](#33-dropdowns)
   - 3.4 [Labels & Separators](#34-labels--separators)
   - 3.5 [Dialogs](#35-dialogs)
   - 3.6 [List Widgets (PluginDialog)](#36-list-widgets-plugindialog)
   - 3.7 [Window Decoration (HeaderBar replacement)](#37-window-decoration-headerbar-replacement)
   - 3.8 [Styling (CSS replacement)](#38-styling-css-replacement)
   - 3.9 [Widget Visibility & Sensitivity](#39-widget-visibility--sensitivity)
4. [Implementation Tasks](#4-implementation-tasks)
   - 4.1 [Build System](#41-build-system)
   - 4.2 [main_linux.cpp](#42-main_linuxcpp)
   - 4.3 [MainWindow](#43-mainwindow)
   - 4.4 [ControlBar](#44-controlbar)
   - 4.5 [PresetBar](#45-presetbar)
   - 4.6 [PluginSlot](#46-pluginslot)
   - 4.7 [ParameterPanel](#47-parameterpanel)
   - 4.8 [PluginDialog](#48-plugindialog)
   - 4.9 [SettingsDialog](#49-settingsdialog)
5. [Signal & Callback Strategy](#5-signal--callback-strategy)
6. [Threading Model](#6-threading-model)
7. [Styling Strategy](#7-styling-strategy)
8. [Testing Checklist](#8-testing-checklist)

---

## 1. Scope & Goals

Produce a functional equivalent of the GTK4 UI using only GTK 2.x APIs, with **no changes to the domain layer**. The user-visible feature set is identical:

- 2×2 grid of plugin slots, each with Add / Bypass / Remove controls and a scrollable parameter panel.
- Bottom control bar: Power toggle, Gain slider, Format/Quality dropdowns, Record toggle, xrun counter, status label.
- Preset bar: named preset dropdown, name entry, Load / Save / Delete buttons.
- Settings dialog: JACK port selection, sample rate / block size display, plugin cache invalidation, preset export/import.
- About dialog.
- Plugin browser dialog with live search filter.

The GTK2 port is the most constrained of the toolkit targets. Several GTK3/GTK4 widget classes do not exist in GTK2 and require non-trivial replacements (see §3). The domain layer is completely unchanged.

---

## 2. Source Layout

Create `src/gtk2/` mirroring `src/gtk3/`. Files that contain no GTK calls should be symlinked from `src/gtk4/`.

```
src/gtk2/
    AppSettings.h/.cpp        ← symlink from gtk4/ (unchanged)
    AudioEngine.h/.cpp        ← symlink from gtk4/ (unchanged)
    JackPortEnum.h/.cpp       ← symlink from gtk4/ (unchanged)
    version.h                 ← symlink from gtk4/ (unchanged)
    MainWindow.h/.cpp         ← port
    ControlBar.h/.cpp         ← port
    PresetBar.h/.cpp          ← port
    PluginSlot.h/.cpp         ← port
    ParameterPanel.h/.cpp     ← port
    PluginDialog.h/.cpp       ← port (largest rewrite: GtkListBox → GtkTreeView)
    SettingsDialog.h/.cpp     ← port
```

The four symlinked files (`AppSettings`, `AudioEngine`, `JackPortEnum`, `version.h`) contain zero GTK calls and compile identically under GTK2.

---

## 3. API Mapping: GTK4 → GTK2

### 3.1 Container / Layout Widgets

| GTK4 / GTK3 | GTK2 replacement | Notes |
|---|---|---|
| `gtk_box_new(GTK_ORIENTATION_HORIZONTAL, n)` | `gtk_hbox_new(FALSE, n)` | `GtkHBox` deprecated in GTK3 but present in GTK2 |
| `gtk_box_new(GTK_ORIENTATION_VERTICAL, n)` | `gtk_vbox_new(FALSE, n)` | `GtkVBox` |
| `gtk_box_pack_start(box, child, e, f, pad)` | Same | Identical in GTK2 |
| `gtk_box_pack_end(box, child, e, f, pad)` | Same | Identical in GTK2 |
| `gtk_grid_new()` | `gtk_table_new(rows, cols, homogeneous)` | `GtkTable` is the GTK2 equivalent |
| `gtk_grid_attach(g, w, c, r, 1, 1)` | `gtk_table_attach(t, w, c, c+1, r, r+1, opts, opts, 0, 0)` | See §4.3 for fill options |
| `gtk_grid_set_row_spacing` | `gtk_table_set_row_spacings` | |
| `gtk_grid_set_column_spacing` | `gtk_table_set_col_spacings` | |
| `gtk_frame_new(label)` | Same | Identical |
| `gtk_container_add(frame, child)` | Same | Identical |
| `gtk_scrolled_window_new(NULL, NULL)` | Same | GTK2 also takes hadjustment/vadjustment |
| `gtk_container_add(sw, child)` | Same | Identical |
| `gtk_window_set_child` / `gtk_container_add(win, child)` | `gtk_container_add(GTK_CONTAINER(win), child)` | Standard GTK2 |

### 3.2 Buttons & Input

| GTK3/GTK4 | GTK2 replacement | Notes |
|---|---|---|
| `gtk_button_new_with_label(label)` | Same | Identical |
| `gtk_toggle_button_new_with_label(label)` | Same | Identical |
| `gtk_check_button_new()` | Same | Identical |
| `gtk_toggle_button_set_active(btn, val)` | Same | Identical |
| `gtk_toggle_button_get_active(btn)` | Same | Identical |
| `gtk_widget_set_sensitive(w, TRUE/FALSE)` | Same | Identical |
| `gtk_scale_new_with_range(orient, min, max, step)` | `gtk_hscale_new_with_range(min, max, step)` | GTK2 has `GtkHScale`/`GtkVScale`; no orientation enum |
| `gtk_range_get_value(range)` | Same | Identical |
| `gtk_range_set_value(range, val)` | Same | Identical |
| `gtk_scale_set_draw_value(scale, FALSE)` | Same | Identical |
| `gtk_entry_new()` | Same | Identical |
| `gtk_entry_get_text(entry)` | Same | Identical in GTK2 (GTK3 adds `gtk_editable_get_text`) |
| `gtk_entry_set_text(entry, text)` | Same | Identical |

### 3.3 Dropdowns

GTK2 does not have `GtkComboBoxText` (added in GTK 2.24) or `GtkDropDown` (GTK4). The available options are:

| Toolkit | Widget | Notes |
|---|---|---|
| GTK4 | `GtkDropDown` + `GtkStringList` | Factory-based |
| GTK3 | `GtkComboBoxText` | Convenience wrapper |
| GTK2 (≥ 2.24) | `GtkComboBoxText` | Available; same API as GTK3 |
| GTK2 (< 2.24) | `GtkComboBox` + `GtkListStore` | Lower-level API |

**Recommended approach:** Target GTK 2.24+ and use `GtkComboBoxText`, which has the same API as GTK3's `GtkComboBoxText`. If support for older GTK2 is needed, use the `GtkComboBox` + `GtkListStore` implementation shown below.

**`GtkComboBoxText` (GTK 2.24+) — preferred:**

```cpp
// Building a simple string dropdown (same as GTK3):
GtkWidget* drop = gtk_combo_box_text_new();
gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(drop), "WAV");
gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(drop), "MP3");
// ...
gtk_combo_box_set_active(GTK_COMBO_BOX(drop), 0);
g_signal_connect(drop, "changed", G_CALLBACK(onFormatChanged), this);
```

**`GtkComboBox` + `GtkListStore` (GTK 2.4–2.22) — fallback:**

```cpp
GtkListStore* store = gtk_list_store_new(1, G_TYPE_STRING);
GtkWidget* drop = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
g_object_unref(store);

GtkCellRenderer* cell = gtk_cell_renderer_text_new();
gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(drop), cell, TRUE);
gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(drop), cell,
    "text", 0, NULL);

const char* items[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
for (const char* item : items) {
    GtkTreeIter iter;
    gtk_list_store_append(GTK_LIST_STORE(store), &iter);
    gtk_list_store_set(GTK_LIST_STORE(store), &iter, 0, item, -1);
}
gtk_combo_box_set_active(GTK_COMBO_BOX(drop), 0);
g_signal_connect(drop, "changed", G_CALLBACK(onFormatChanged), this);
```

**Reading selected index (same for both):**

```cpp
int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(drop));
```

**Preset bar — searchable dropdown:**

GTK2 has no built-in searchable `GtkDropDown`. Use `GtkComboBoxEntry` (GTK 2.4+) which combines a `GtkComboBox` with a text entry, allowing the user to type a name as well as select from the list:

```cpp
// PresetBar dropdown with editable entry:
GtkWidget* presetDrop_ = gtk_combo_box_entry_new_text();
// Populate:
gtk_combo_box_append_text(GTK_COMBO_BOX(presetDrop_), "My Preset");
// Get typed/selected value:
GtkWidget* entry = gtk_bin_get_child(GTK_BIN(presetDrop_));
const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
```

### 3.4 Labels & Separators

| GTK3 | GTK2 replacement | Notes |
|---|---|---|
| `gtk_label_new(text)` | Same | Identical |
| `gtk_label_set_text(lbl, text)` | Same | Identical |
| `gtk_label_set_xalign(lbl, 0.0)` | `gtk_misc_set_alignment(GTK_MISC(lbl), 0.0f, 0.5f)` | `gtk_label_set_xalign` not available in GTK2 |
| `gtk_label_set_ellipsize(lbl, mode)` | Same | Available since GTK 2.6 |
| `gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)` | `gtk_hseparator_new()` | GTK2 has distinct `GtkHSeparator`/`GtkVSeparator` types |
| `gtk_separator_new(GTK_ORIENTATION_VERTICAL)` | `gtk_vseparator_new()` | |

### 3.5 Dialogs

| GTK4 / GTK3 | GTK2 replacement | Notes |
|---|---|---|
| `GtkMessageDialog` | `GtkMessageDialog` | Available since GTK 2.4; same constructor |
| `gtk_dialog_run(dialog)` | Same | Identical |
| `gtk_widget_destroy(dialog)` | Same | Identical |
| `GtkAboutDialog` | `GtkAboutDialog` | Available since GTK 2.6 |
| `GtkFileChooserDialog` + `gtk_dialog_run()` | Same | Available since GTK 2.4 |
| `gtk_file_chooser_get_filename()` | Same | Identical |
| `gtk_window_destroy(win)` | `gtk_widget_destroy(win)` | GTK2 uses `gtk_widget_destroy` |

### 3.6 List Widgets (PluginDialog)

GTK2 does **not** have `GtkListBox` (added in GTK 3.10). The replacement is a `GtkTreeView` backed by a `GtkListStore`.

| GTK3 | GTK2 replacement |
|---|---|
| `GtkListBox` | `GtkTreeView` + `GtkListStore` |
| `GtkListBoxRow` | `GtkTreeIter` row in the store |
| `gtk_list_box_append(lb, row)` | `gtk_list_store_append(store, &iter)` + `gtk_list_store_set(...)` |
| `gtk_list_box_selected_foreach` / `gtk_list_box_get_selected_row()` | `gtk_tree_selection_get_selected(sel, &model, &iter)` |
| `row-activated` signal | `row-activated` signal on `GtkTreeView` |
| `GtkSearchEntry` | `GtkEntry` with `changed` signal |

See §4.8 for the full `PluginDialog` `GtkTreeView` implementation.

### 3.7 Window Decoration (HeaderBar replacement)

GTK2 has no `GtkHeaderBar`. The header buttons (About, Settings) must be placed in a `GtkMenuBar` or an `HBox` toolbar below the title bar.

**Recommended approach — `GtkMenuBar`:**

```cpp
// In MainWindow::buildWidgets():
GtkWidget* menuBar = gtk_menu_bar_new();

// "View" menu → About
GtkWidget* viewMenu    = gtk_menu_new();
GtkWidget* viewItem    = gtk_menu_item_new_with_label("View");
GtkWidget* aboutItem   = gtk_menu_item_new_with_label("About");
GtkWidget* settingsItem = gtk_menu_item_new_with_label("Settings");
gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), settingsItem);
gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), aboutItem);
gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewItem), viewMenu);
gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), viewItem);

g_signal_connect_swapped(aboutItem,    "activate",
    G_CALLBACK(+[](MainWindow* self) { self->showAboutDialog(); }), this);
g_signal_connect_swapped(settingsItem, "activate",
    G_CALLBACK(+[](MainWindow* self) { self->openSettings(); }), this);

// Pack at top of root vbox:
gtk_box_pack_start(GTK_BOX(root), menuBar, FALSE, FALSE, 0);
```

**Alternative — `GtkToolbar` with buttons:**

A `GtkToolbar` with `GTK_TOOLBAR_BOTH_HORIZ` style can approximate the HeaderBar look. Less desirable because toolbars sit below the title bar and look distinctly different from the GTK4 design.

### 3.8 Styling (CSS replacement)

GTK2 does not have `GtkCssProvider`. Style application uses **GTK RC** (resource string) parsed via `gtk_rc_parse_string()`.

**RC string equivalent of the GTK4/GTK3 CSS:**

```cpp
// In MainWindow::loadStyle():
static const char kAppRcString[] = R"(
style "opiqo-plugin-name" {
    font_desc = "Bold"
}
widget_class "*GtkLabel.plugin-name" style "opiqo-plugin-name"

style "opiqo-destructive" {
    bg[NORMAL]   = "#cc0000"
    bg[PRELIGHT] = "#ff2222"
    fg[NORMAL]   = "#ffffff"
    fg[PRELIGHT] = "#ffffff"
}
widget_class "*GtkButton.destructive-action" style "opiqo-destructive"

style "opiqo-suggested" {
    bg[NORMAL]   = "#1a6ebd"
    bg[PRELIGHT] = "#2080d0"
    fg[NORMAL]   = "#ffffff"
    fg[PRELIGHT] = "#ffffff"
}
widget_class "*GtkButton.suggested-action" style "opiqo-suggested"
)";

gtk_rc_parse_string(kAppRcString);
```

**Per-widget name for style targeting:**

```cpp
// Name the widget so the RC pattern matches:
gtk_widget_set_name(deleteButton_, "destructive-action");
```

> **Note:** GTK RC widget class patterns (`widget_class "*GtkButton.name"`) are GTK2-specific and require setting the widget name (not a CSS class). The `gtk_widget_set_name` → `widget "*name"` pattern is the closest equivalent to `gtk_widget_add_css_class`.

**Alternative — `gtk_widget_modify_*`:**

For one-off colour/font changes, use `gtk_widget_modify_bg`, `gtk_widget_modify_fg`, `gtk_widget_modify_font` directly on the widget. These are deprecated in GTK3 but are the idiomatic GTK2 per-widget style setters.

```cpp
// Set delete button background red:
GdkColor red = { 0, 0xcccc, 0, 0 };
gtk_widget_modify_bg(deleteButton_, GTK_STATE_NORMAL, &red);
```

### 3.9 Widget Visibility & Sensitivity

| GTK3 | GTK2 | Notes |
|---|---|---|
| `gtk_widget_set_visible(w, TRUE)` | `gtk_widget_show(w)` | |
| `gtk_widget_set_visible(w, FALSE)` | `gtk_widget_hide(w)` | |
| `gtk_widget_show_all(w)` | Same | Identical |
| `gtk_widget_set_sensitive(w, val)` | Same | Identical |

---

## 4. Implementation Tasks

### 4.1 Build System

**In `CMakeLists.txt`**, add a new configure preset and build target for GTK2:

```cmake
if(OPIQO_TARGET_PLATFORM STREQUAL "linux-gtk2")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

    set(GTK2_SRCS
        src/main_linux.cpp
        src/gtk2/MainWindow.cpp
        src/gtk2/ControlBar.cpp
        src/gtk2/PresetBar.cpp
        src/gtk2/PluginSlot.cpp
        src/gtk2/ParameterPanel.cpp
        src/gtk2/PluginDialog.cpp
        src/gtk2/SettingsDialog.cpp
        src/gtk2/AppSettings.cpp
        src/gtk2/AudioEngine.cpp
        src/gtk2/JackPortEnum.cpp
        # shared domain sources:
        src/LiveEffectEngine.cpp
        src/FileWriter.cpp
        src/LockFreeQueue.cpp
    )

    add_executable(opiqo-gtk2 ${GTK2_SRCS})
    target_include_directories(opiqo-gtk2 PRIVATE src src/gtk2
        ${GTK2_INCLUDE_DIRS})
    target_compile_options(opiqo-gtk2 PRIVATE
        ${GTK2_CFLAGS_OTHER}
        -DOPIQO_GTK2)
    target_link_libraries(opiqo-gtk2
        ${GTK2_LIBRARIES} jack lilv-0 sndfile opus opusenc mp3lame FLAC
        -ldl -lpthread)
endif()
```

**In `CMakePresets.json`**, add:

```json
{
    "name": "linux-gtk2",
    "displayName": "Linux GTK2 (JACK)",
    "binaryDir": "${sourceDir}/build-linux-gtk2",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "OPIQO_TARGET_PLATFORM": "linux-gtk2"
    }
}
```

**In `Makefile`** (if used for convenience targets), add:

```makefile
gtk2:
	cmake --preset linux-gtk2 && cmake --build build-linux-gtk2 --parallel
```

---

### 4.2 main_linux.cpp

The entry point uses `GtkApplication` which is available in GTK 2.x via the `gio` package. However, `GtkApplication` was introduced in GTK 3.0. For GTK2, use the classic `gtk_init` + `gtk_main` pattern:

```cpp
// src/gtk2/main_linux.cpp  (replaces src/main_linux.cpp for this build)
#include <gtk/gtk.h>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    MainWindow win;
    gtk_widget_show_all(win.window());

    gtk_main();
    return 0;
}
```

> **Important:** GTK2 does not have `GtkApplication`. `MainWindow` must **not** take a `GtkApplication*` parameter. Use a no-argument constructor instead. The `g_application_*` / `GtkApplication` ownership and `activate` signal pattern is GTK3+.

**`MainWindow` constructor signature change:**

```cpp
// GTK4 / GTK3:
MainWindow(GtkApplication* app);

// GTK2:
MainWindow();
```

The window is created with `gtk_window_new(GTK_WINDOW_TOPLEVEL)` directly.

---

### 4.3 MainWindow

**Header changes:**

```cpp
// GTK2: drop GtkApplication* param; use gtk_window_new directly
class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    GtkWidget* window() const { return window_; }
    // ... rest same as GTK3 ...
};
```

**Constructor:**

```cpp
MainWindow::MainWindow() {
    // Domain layer setup (identical to GTK4):
    engine_   = std::make_unique<LiveEffectEngine>();
    settings_ = AppSettings::load();
    // ...

    // Create window (GTK2):
    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "Opiqo");
    gtk_window_set_default_size(GTK_WINDOW(window_), 960, 680);
    g_signal_connect_swapped(window_, "destroy",
        G_CALLBACK(gtk_main_quit), nullptr);

    loadStyle();
    buildWidgets();
    // Note: gtk_widget_show_all is called by main_linux.cpp
}
```

**`buildWidgets()`:**

```cpp
// GTK2: GtkVBox as root, GtkMenuBar at top, GtkTable for slots
GtkWidget* root = gtk_vbox_new(FALSE, 0);
gtk_container_add(GTK_CONTAINER(window_), root);

// Menu bar replaces GtkHeaderBar:
buildMenuBar(root);

// 2×2 slot table (GtkTable replaces GtkGrid):
slotTable_ = gtk_table_new(2, 2, TRUE);   // 2 rows, 2 cols, homogeneous
gtk_table_set_row_spacings(GTK_TABLE(slotTable_), 4);
gtk_table_set_col_spacings(GTK_TABLE(slotTable_), 4);
gtk_box_pack_start(GTK_BOX(root), slotTable_, TRUE, TRUE, 0);

// Slot grid: attach 4 PluginSlot widgets
for (int r = 0; r < 2; ++r) {
    for (int c = 0; c < 2; ++c) {
        int slot = r * 2 + c + 1;
        slots_[slot - 1] = new PluginSlot(slot, window_);
        gtk_table_attach(GTK_TABLE(slotTable_),
            slots_[slot - 1]->widget(),
            c, c + 1, r, r + 1,
            GtkAttachOptions(GTK_EXPAND | GTK_FILL),
            GtkAttachOptions(GTK_EXPAND | GTK_FILL),
            2, 2);
    }
}

// Separator:
gtk_box_pack_start(GTK_BOX(root), gtk_hseparator_new(), FALSE, FALSE, 2);

// PresetBar and ControlBar (append themselves to root):
presetBar_  = std::make_unique<PresetBar>(root);
controlBar_ = std::make_unique<ControlBar>(root);
```

**`buildMenuBar(GtkWidget* root)`:**

```cpp
void MainWindow::buildMenuBar(GtkWidget* root) {
    GtkWidget* menuBar = gtk_menu_bar_new();

    // File menu (for consistency):
    GtkWidget* fileMenu  = gtk_menu_new();
    GtkWidget* fileItem  = gtk_menu_item_new_with_label("File");
    GtkWidget* quitItem  = gtk_menu_item_new_with_label("Quit");
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), quitItem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileItem), fileMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), fileItem);
    g_signal_connect_swapped(quitItem, "activate",
        G_CALLBACK(gtk_main_quit), nullptr);

    // View menu:
    GtkWidget* viewMenu     = gtk_menu_new();
    GtkWidget* viewItem     = gtk_menu_item_new_with_label("View");
    GtkWidget* settingsItem = gtk_menu_item_new_with_label("Settings…");
    GtkWidget* sepItem      = gtk_separator_menu_item_new();
    GtkWidget* aboutItem    = gtk_menu_item_new_with_label("About");
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), settingsItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), sepItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), aboutItem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewItem), viewMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), viewItem);

    g_signal_connect_swapped(settingsItem, "activate",
        G_CALLBACK(+[](MainWindow* self) { self->openSettings(); }), this);
    g_signal_connect_swapped(aboutItem, "activate",
        G_CALLBACK(+[](MainWindow* self) { self->showAboutDialog(); }), this);

    gtk_box_pack_start(GTK_BOX(root), menuBar, FALSE, FALSE, 0);
}
```

**`loadStyle()`:**

```cpp
void MainWindow::loadStyle() {
    // GTK2: use gtk_rc_parse_string instead of GtkCssProvider
    static const char kAppRc[] = R"(
style "opiqo-delete-btn" {
    bg[NORMAL]   = "#cc0000"
    bg[PRELIGHT] = "#ff3333"
    fg[NORMAL]   = "#ffffff"
    fg[PRELIGHT] = "#ffffff"
}
widget "*.delete-btn" style "opiqo-delete-btn"

style "opiqo-confirm-btn" {
    bg[NORMAL]   = "#1a6ebd"
    bg[PRELIGHT] = "#2080d0"
    fg[NORMAL]   = "#ffffff"
    fg[PRELIGHT] = "#ffffff"
}
widget "*.confirm-btn" style "opiqo-confirm-btn"
)";
    gtk_rc_parse_string(kAppRc);
}
```

**`onAddPlugin()` alert dialog:**

```cpp
GtkWidget* alert = gtk_message_dialog_new(
    GTK_WINDOW(window_),
    GTK_DIALOG_FLAGS(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
    GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
    "Start the engine before loading plugins.");
gtk_dialog_run(GTK_DIALOG(alert));
gtk_widget_destroy(alert);
```

**`showAboutDialog()`:**

```cpp
// GtkAboutDialog is available since GTK 2.6:
GtkWidget* about = gtk_about_dialog_new();
gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), "Opiqo");
gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), APP_VERSION);
gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), "\"" APP_CODENAME "\"");
const char* authors[] = { "Opiqo contributors", nullptr };
gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(window_));
gtk_dialog_run(GTK_DIALOG(about));
gtk_widget_destroy(about);
```

---

### 4.4 ControlBar

**`buildWidgets()` constructor takes `GtkWidget* parent_box`** (same as GTK3):

```cpp
ControlBar::ControlBar(GtkWidget* parent_box) {
    bar_ = gtk_hbox_new(FALSE, 4);          // GtkHBox, not gtk_box_new
    buildWidgets();
    gtk_box_pack_start(GTK_BOX(parent_box), bar_, FALSE, FALSE, 0);
}
```

**Complete `buildWidgets()`:**

```cpp
void ControlBar::buildWidgets() {
    // Power toggle:
    powerToggle_ = gtk_toggle_button_new_with_label("Power");
    gtk_box_pack_start(GTK_BOX(bar_), powerToggle_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 2);

    // Gain label + scale:
    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_label_new("Gain"), FALSE, FALSE, 0);
    gainScale_ = gtk_hscale_new_with_range(0.0, 2.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(gainScale_), FALSE);
    gtk_widget_set_size_request(gainScale_, 120, -1);
    gtk_box_pack_start(GTK_BOX(bar_), gainScale_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 2);

    // Format label + combo:
    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_label_new("Format"), FALSE, FALSE, 0);
    formatDrop_ = gtk_combo_box_text_new();
    const char* fmts[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
    for (const char* f : fmts)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(formatDrop_), f);
    gtk_combo_box_set_active(GTK_COMBO_BOX(formatDrop_), 0);
    gtk_box_pack_start(GTK_BOX(bar_), formatDrop_, FALSE, FALSE, 0);

    // Quality box (hidden for lossless):
    qualityBox_ = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(qualityBox_),
        gtk_label_new("Quality"), FALSE, FALSE, 0);
    qualityDrop_ = gtk_combo_box_text_new();
    for (int i = 0; i <= 9; ++i) {
        char buf[4]; snprintf(buf, sizeof(buf), "%d", i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(qualityDrop_), buf);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(qualityDrop_), 5);
    gtk_box_pack_start(GTK_BOX(qualityBox_), qualityDrop_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar_), qualityBox_, FALSE, FALSE, 0);
    gtk_widget_hide(qualityBox_);  // GTK2: hide() instead of set_visible(FALSE)

    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 2);

    // Record toggle:
    recordToggle_ = gtk_toggle_button_new_with_label("● Record");
    gtk_box_pack_start(GTK_BOX(bar_), recordToggle_, FALSE, FALSE, 0);

    // Expanding spacer:
    GtkWidget* spacer = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar_), spacer, TRUE, TRUE, 0);

    // xrun label, status label (right-aligned via pack_end):
    xrunLabel_   = gtk_label_new("Xruns: 0");
    statusLabel_ = gtk_label_new("Stopped");
    gtk_box_pack_end(GTK_BOX(bar_), statusLabel_, FALSE, FALSE, 4);
    gtk_box_pack_end(GTK_BOX(bar_), xrunLabel_,   FALSE, FALSE, 4);

    // Signals:
    g_signal_connect_swapped(powerToggle_, "toggled",
        G_CALLBACK(+[](ControlBar* self) { self->onPowerToggled(); }), this);
    g_signal_connect_swapped(gainScale_, "value-changed",
        G_CALLBACK(+[](ControlBar* self) { self->onGainChanged(); }), this);
    g_signal_connect_swapped(formatDrop_, "changed",
        G_CALLBACK(+[](ControlBar* self) { self->onFormatChanged(); }), this);
    g_signal_connect_swapped(recordToggle_, "toggled",
        G_CALLBACK(+[](ControlBar* self) { self->onRecordToggled(); }), this);
}
```

**`onFormatChanged()`:**

```cpp
void ControlBar::onFormatChanged() {
    if (suppressSignals_) return;
    int fmt = gtk_combo_box_get_active(GTK_COMBO_BOX(formatDrop_));
    bool lossy = (fmt == 1 || fmt == 2 || fmt == 3);  // MP3, OGG, OPUS
    if (lossy)
        gtk_widget_show(qualityBox_);
    else
        gtk_widget_hide(qualityBox_);
}
```

**`setPowerState` / `setRecordingActive`:**

```cpp
void ControlBar::setPowerState(bool on) {
    suppressSignals_ = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(powerToggle_), on);
    suppressSignals_ = false;
}
```

---

### 4.5 PresetBar

```cpp
PresetBar::PresetBar(GtkWidget* parent_box) {
    bar_ = gtk_hbox_new(FALSE, 4);

    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_label_new("Preset:"), FALSE, FALSE, 0);

    // GtkComboBoxEntry: combined dropdown + text entry
    // (provides the "searchable" behaviour via typing)
    presetDrop_ = gtk_combo_box_entry_new_text();
    gtk_widget_set_size_request(presetDrop_, 180, -1);
    gtk_box_pack_start(GTK_BOX(bar_), presetDrop_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_label_new("Name:"), FALSE, FALSE, 0);
    nameEntry_ = gtk_entry_new();
    gtk_widget_set_size_request(nameEntry_, 140, -1);
    gtk_box_pack_start(GTK_BOX(bar_), nameEntry_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 2);

    loadBtn_   = gtk_button_new_with_label("Load");
    saveBtn_   = gtk_button_new_with_label("Save");
    deleteBtn_ = gtk_button_new_with_label("Delete");
    gtk_box_pack_start(GTK_BOX(bar_), loadBtn_,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar_), saveBtn_,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar_), deleteBtn_, FALSE, FALSE, 0);

    g_signal_connect_swapped(loadBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) { if (self->loadCb_) self->loadCb_(); }),
        this);
    g_signal_connect_swapped(saveBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) {
            if (self->saveCb_) self->saveCb_(self->getCurrentName());
        }), this);
    g_signal_connect_swapped(deleteBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) { if (self->deleteCb_) self->deleteCb_(); }),
        this);

    // When user selects from dropdown, copy name to nameEntry_:
    g_signal_connect_swapped(presetDrop_, "changed",
        G_CALLBACK(+[](PresetBar* self) { self->onDropdownChanged(); }), this);

    gtk_box_pack_start(GTK_BOX(parent_box), bar_, FALSE, FALSE, 0);
}
```

**`setPresetNames()`:**

```cpp
void PresetBar::setPresetNames(const std::vector<std::string>& names) {
    suppressSignals_ = true;

    // Remove all current entries from GtkComboBoxEntry:
    // GtkComboBoxEntry backed by GtkListStore column 0 (text).
    GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(presetDrop_));
    gtk_list_store_clear(GTK_LIST_STORE(model));

    for (const auto& name : names)
        gtk_combo_box_append_text(GTK_COMBO_BOX(presetDrop_), name.c_str());

    suppressSignals_ = false;
}
```

**`getSelectedIndex()` for GtkComboBoxEntry:**

```cpp
int PresetBar::getSelectedIndex() const {
    return gtk_combo_box_get_active(GTK_COMBO_BOX(presetDrop_));
}
```

**`getCurrentName()`:**

```cpp
std::string PresetBar::getCurrentName() const {
    // getText from the embedded GtkEntry child of GtkComboBoxEntry:
    GtkWidget* entry = gtk_bin_get_child(GTK_BIN(presetDrop_));
    return gtk_entry_get_text(GTK_ENTRY(entry));
}
```

**`onDropdownChanged()`:**

```cpp
void PresetBar::onDropdownChanged() {
    if (suppressSignals_) return;
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(presetDrop_));
    if (idx < 0) return;
    // Copy selected name into nameEntry_:
    GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(presetDrop_));
    GtkTreeIter iter;
    if (gtk_tree_model_iter_nth_child(model, &iter, nullptr, idx)) {
        gchar* name = nullptr;
        gtk_tree_model_get(model, &iter, 0, &name, -1);
        gtk_entry_set_text(GTK_ENTRY(nameEntry_), name ? name : "");
        g_free(name);
    }
}
```

---

### 4.6 PluginSlot

```cpp
PluginSlot::PluginSlot(int slot, GtkWidget* parent_window)
    : slot_(slot), parentWindow_(parent_window) {
    paramPanel_ = new ParameterPanel(parent_window);
    buildWidgets();
}
```

**`buildWidgets()`:**

```cpp
void PluginSlot::buildWidgets() {
    char title[32];
    snprintf(title, sizeof(title), "Slot %d", slot_);

    frame_ = gtk_frame_new(nullptr);

    GtkWidget* outerBox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame_), outerBox);

    // Header row:
    headerBox_ = gtk_hbox_new(FALSE, 2);
    nameLabel_ = gtk_label_new(title);
    gtk_misc_set_alignment(GTK_MISC(nameLabel_), 0.0f, 0.5f);

    addButton_    = gtk_button_new_with_label("+ Add");
    bypassButton_ = gtk_toggle_button_new_with_label("Bypass");
    deleteButton_ = gtk_button_new_with_label("× Remove");

    // Style the delete button:
    gtk_widget_set_name(deleteButton_, "delete-btn");

    gtk_box_pack_start(GTK_BOX(headerBox_), nameLabel_,    TRUE,  TRUE,  4);
    gtk_box_pack_start(GTK_BOX(headerBox_), addButton_,    FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(headerBox_), bypassButton_, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(headerBox_), deleteButton_, FALSE, FALSE, 2);

    gtk_widget_set_sensitive(bypassButton_, FALSE);
    gtk_widget_set_sensitive(deleteButton_, FALSE);

    gtk_box_pack_start(GTK_BOX(outerBox), headerBox_, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(outerBox), gtk_hseparator_new(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outerBox), paramPanel_->widget(), TRUE, TRUE, 0);

    // Signals:
    g_signal_connect_swapped(addButton_, "clicked",
        G_CALLBACK(+[](PluginSlot* self) {
            if (self->addCb_) self->addCb_(self->slot_);
        }), this);
    g_signal_connect_swapped(bypassButton_, "toggled",
        G_CALLBACK(+[](PluginSlot* self) {
            bool active = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(self->bypassButton_));
            if (self->bypassCb_) self->bypassCb_(self->slot_, active);
        }), this);
    g_signal_connect_swapped(deleteButton_, "clicked",
        G_CALLBACK(+[](PluginSlot* self) {
            if (self->deleteCb_) self->deleteCb_(self->slot_);
        }), this);
}
```

**`onPluginAdded` / `onPluginCleared`:** Logic identical to GTK3; `gtk_label_set_text`, `gtk_widget_set_sensitive`, `gtk_toggle_button_set_active` all unchanged.

---

### 4.7 ParameterPanel

**Constructor:**

```cpp
ParameterPanel::ParameterPanel(GtkWidget* parent_window)
    : parentWindow_(parent_window) {
    scroll_ = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll_, -1, 180);

    box_ = gtk_vbox_new(FALSE, 2);

    // GTK2: ScrolledWindow with a viewport:
    gtk_scrolled_window_add_with_viewport(
        GTK_SCROLLED_WINDOW(scroll_), box_);
}
```

> **GTK2 note:** `gtk_container_add` on a `GtkScrolledWindow` with a non-scrollable widget requires a `GtkViewport` wrapper. Use `gtk_scrolled_window_add_with_viewport()` for plain boxes; this is removed in GTK3 (where `GtkScrolledWindow` handles it automatically).

**`build()` — row construction:**

```cpp
// Per port row:
GtkWidget* row = gtk_hbox_new(FALSE, 4);
GtkWidget* lbl = gtk_label_new(port.label.c_str());
gtk_widget_set_size_request(lbl, 150, -1);
gtk_misc_set_alignment(GTK_MISC(lbl), 0.0f, 0.5f);
gtk_box_pack_start(GTK_BOX(row), lbl,  FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(row), ctrl, TRUE,  TRUE,  0);
gtk_box_pack_start(GTK_BOX(box_), row, FALSE, FALSE, 2);
gtk_box_pack_start(GTK_BOX(box_),
    gtk_hseparator_new(), FALSE, FALSE, 0);
```

**`GtkHScale` for Float ports:**

```cpp
// GTK2: gtk_hscale_new_with_range instead of gtk_scale_new_with_range:
double step = (port.max - port.min) / 200.0;
GtkWidget* ctrl = gtk_hscale_new_with_range(port.min, port.max, step);
gtk_scale_set_draw_value(GTK_SCALE(ctrl), TRUE);
gtk_range_set_value(GTK_RANGE(ctrl), port.defaultVal);
```

**`GtkComboBoxText` for enum ports (GTK 2.24+):**

Same as §3.3. Signal is `"changed"` on the `GtkComboBox`.

**`clear()` — removing children:**

```cpp
GList* children = gtk_container_get_children(GTK_CONTAINER(box_));
for (GList* l = children; l; l = l->next)
    gtk_container_remove(GTK_CONTAINER(box_), GTK_WIDGET(l->data));
g_list_free(children);
// Free ControlData entries:
for (auto* cd : controlDataList_) delete cd;
controlDataList_.clear();
```

**File browse — synchronous `GtkFileChooserDialog`:**

```cpp
GtkWidget* dialog = gtk_file_chooser_dialog_new(
    "Open File",
    GTK_WINDOW(self->parentWindow_),
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
    nullptr);

if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if (path && self->fileCb_)
        self->fileCb_(cd->writableUri, std::string(path));
    g_free(path);
}
gtk_widget_destroy(dialog);
```

> **GTK2 stock icons:** use `GTK_STOCK_CANCEL` / `GTK_STOCK_OPEN` instead of `"_Cancel"` / `"_Open"` (stock deprecated in GTK3, removed in GTK4).

---

### 4.8 PluginDialog

This class requires the most significant rewrite because `GtkListBox` is a GTK3+ widget. The replacement is `GtkTreeView` backed by a `GtkListStore`.

**Store layout:**

| Column | Type | Content |
|---|---|---|
| 0 | `G_TYPE_STRING` | Plugin name |
| 1 | `G_TYPE_STRING` | Author + URI label |
| 2 | `G_TYPE_STRING` | Plugin URI (hidden; used for selection) |

**`buildWidgets()`:**

```cpp
void PluginDialog::buildWidgets() {
    dialog_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog_), "Add Plugin");
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 540, 480);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent_);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);

    GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(dialog_), vbox);

    // Search entry (plain GtkEntry):
    searchEntry_ = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(searchEntry_), "");
    gtk_box_pack_start(GTK_BOX(vbox), searchEntry_, FALSE, FALSE, 0);

    // GtkListStore:
    store_ = gtk_list_store_new(3,
        G_TYPE_STRING,   // 0: name
        G_TYPE_STRING,   // 1: author/URI label
        G_TYPE_STRING);  // 2: URI (hidden)

    // GtkTreeView (no headers needed):
    treeView_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_));
    g_object_unref(store_);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView_), FALSE);

    // Column 0 — name:
    GtkCellRenderer* nameCell = gtk_cell_renderer_text_new();
    g_object_set(nameCell, "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_tree_view_insert_column_with_attributes(
        GTK_TREE_VIEW(treeView_), -1, "Name", nameCell, "text", 0, nullptr);

    // Column 1 — author/uri:
    GtkCellRenderer* subCell = gtk_cell_renderer_text_new();
    g_object_set(subCell,
        "foreground", "gray50",
        "ellipsize", PANGO_ELLIPSIZE_END,
        nullptr);
    gtk_tree_view_insert_column_with_attributes(
        GTK_TREE_VIEW(treeView_), -1, "Info", subCell, "text", 1, nullptr);

    // Scroll:
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), treeView_);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    // Button row:
    GtkWidget* btnBox  = gtk_hbox_new(FALSE, 6);
    GtkWidget* spacer  = gtk_hbox_new(FALSE, 0);
    cancelBtn_         = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    confirmBtn_        = gtk_button_new_with_label("Add Plugin");
    gtk_widget_set_name(confirmBtn_, "confirm-btn");
    gtk_box_pack_start(GTK_BOX(btnBox), spacer,     TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(btnBox), cancelBtn_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btnBox), confirmBtn_,FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btnBox, FALSE, FALSE, 0);

    // Signals:
    g_signal_connect_swapped(searchEntry_, "changed",
        G_CALLBACK(+[](PluginDialog* self) {
            self->rebuildList(gtk_entry_get_text(GTK_ENTRY(self->searchEntry_)));
        }), this);
    g_signal_connect_swapped(treeView_, "row-activated",
        G_CALLBACK(+[](PluginDialog* self,
                        GtkTreePath*, GtkTreeViewColumn*) {
            self->onConfirm();
        }), this);
    g_signal_connect_swapped(cancelBtn_, "clicked",
        G_CALLBACK(+[](PluginDialog* self) { self->onCancel(); }), this);
    g_signal_connect_swapped(confirmBtn_, "clicked",
        G_CALLBACK(+[](PluginDialog* self) { self->onConfirm(); }), this);

    // Populate initially:
    rebuildList("");
}
```

**`rebuildList(const std::string& filter)`:**

```cpp
void PluginDialog::rebuildList(const std::string& filter) {
    gtk_list_store_clear(store_);
    std::string lf = filter;
    std::transform(lf.begin(), lf.end(), lf.begin(), ::tolower);

    for (const auto& entry : allPlugins_) {
        std::string ln = entry.name, lu = entry.uri;
        std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
        std::transform(lu.begin(), lu.end(), lu.begin(), ::tolower);

        if (!lf.empty() &&
            ln.find(lf) == std::string::npos &&
            lu.find(lf) == std::string::npos)
            continue;

        std::string sub = entry.author + " • " + entry.uri;
        GtkTreeIter iter;
        gtk_list_store_append(store_, &iter);
        gtk_list_store_set(store_, &iter,
            0, entry.name.c_str(),
            1, sub.c_str(),
            2, entry.uri.c_str(),
            -1);
    }
}
```

**`onConfirm()`:**

```cpp
void PluginDialog::onConfirm() {
    GtkTreeSelection* sel = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(treeView_));
    GtkTreeIter iter;
    GtkTreeModel* model = nullptr;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
        return;

    gchar* uri = nullptr;
    gtk_tree_model_get(model, &iter, 2, &uri, -1);
    if (uri && confirmCb_)
        confirmCb_(std::string(uri));
    g_free(uri);

    gtk_widget_destroy(dialog_);
    delete this;
}
```

**`onCancel()`:**

```cpp
void PluginDialog::onCancel() {
    gtk_widget_destroy(dialog_);
    delete this;
}
```

---

### 4.9 SettingsDialog

`GtkNotebook` is available identically in GTK2. The main changes are:
- Replace `GtkDropDown` with `GtkComboBoxText` for port selectors.
- Replace async `GtkFileDialog` with synchronous `GtkFileChooserDialog`.
- Replace `GtkHeaderBar` title with `gtk_window_set_title`.
- Use `gtk_vseparator_new()` / `gtk_hseparator_new()`.

**Construction:**

```cpp
SettingsDialog::SettingsDialog(GtkWidget* parent_window)
    : parentWindow_(parent_window) {

    dialog_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog_), "Settings");
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 480, 360);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_transient_for(
        GTK_WINDOW(dialog_), GTK_WINDOW(parent_window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);

    g_signal_connect(dialog_, "delete-event",
        G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

    GtkWidget* nb = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(dialog_), nb);

    buildAudioTab(nb);
    buildPresetsTab(nb);
}
```

**`buildAudioTab()`:**

```cpp
void SettingsDialog::buildAudioTab(GtkWidget* nb) {
    GtkWidget* vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    // Port rows (table layout):
    GtkWidget* tbl = gtk_table_new(6, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(tbl), 6);
    gtk_table_set_col_spacings(GTK_TABLE(tbl), 8);

    const char* portLabels[] = {
        "Capture L:", "Capture R:", "Playback L:", "Playback R:"
    };
    GtkWidget** portDrops[] = {&capL_, &capR_, &pbL_, &pbR_};

    for (int i = 0; i < 4; ++i) {
        GtkWidget* lbl = gtk_label_new(portLabels[i]);
        gtk_misc_set_alignment(GTK_MISC(lbl), 1.0f, 0.5f);
        *portDrops[i] = gtk_combo_box_text_new();
        gtk_table_attach(GTK_TABLE(tbl), lbl,
            0, 1, i, i+1,
            GTK_FILL, GTK_FILL, 0, 0);
        gtk_table_attach(GTK_TABLE(tbl), *portDrops[i],
            1, 2, i, i+1,
            GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0, 0);
    }

    // Sample rate and block size (read-only labels):
    GtkWidget* srLbl = gtk_label_new("Sample Rate:");
    gtk_misc_set_alignment(GTK_MISC(srLbl), 1.0f, 0.5f);
    srLabel_ = gtk_label_new("—");
    gtk_misc_set_alignment(GTK_MISC(srLabel_), 0.0f, 0.5f);
    gtk_table_attach(GTK_TABLE(tbl), srLbl,
        0, 1, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(tbl), srLabel_,
        1, 2, 4, 5, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0, 0);

    GtkWidget* bsLbl = gtk_label_new("Block Size:");
    gtk_misc_set_alignment(GTK_MISC(bsLbl), 1.0f, 0.5f);
    bsLabel_ = gtk_label_new("—");
    gtk_misc_set_alignment(GTK_MISC(bsLabel_), 0.0f, 0.5f);
    gtk_table_attach(GTK_TABLE(tbl), bsLbl,
        0, 1, 5, 6, GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(tbl), bsLabel_,
        1, 2, 5, 6, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0, 0);

    gtk_box_pack_start(GTK_BOX(vbox), tbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 4);

    // Apply button:
    GtkWidget* applyBtn = gtk_button_new_with_label("Apply");
    g_signal_connect_swapped(applyBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onApply(); }), this);
    gtk_box_pack_start(GTK_BOX(vbox), applyBtn, FALSE, FALSE, 0);

    GtkWidget* cacheBtn = gtk_button_new_with_label("Delete Plugin Cache");
    gtk_widget_set_name(cacheBtn, "delete-btn");
    g_signal_connect_swapped(cacheBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onDeleteCache(); }), this);
    gtk_box_pack_start(GTK_BOX(vbox), cacheBtn, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox,
        gtk_label_new("Audio"));
}
```

**`buildPresetsTab()`:**

```cpp
void SettingsDialog::buildPresetsTab(GtkWidget* nb) {
    GtkWidget* vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    GtkWidget* exportBtn = gtk_button_new_with_label("Export Preset…");
    GtkWidget* importBtn = gtk_button_new_with_label("Import Preset…");

    g_signal_connect_swapped(exportBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onExport(); }), this);
    g_signal_connect_swapped(importBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onImport(); }), this);

    gtk_box_pack_start(GTK_BOX(vbox), exportBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), importBtn, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox,
        gtk_label_new("Presets"));
}
```

**`onExport()`:**

```cpp
void SettingsDialog::onExport() {
    // Synchronous file save dialog (GTK2 / GTK3 pattern):
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Export Preset",
        GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_SAVE,   GTK_RESPONSE_ACCEPT,
        nullptr);
    gtk_file_chooser_set_current_name(
        GTK_FILE_CHOOSER(dlg), "opiqo-preset.json");

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (path && exportCb_) {
            std::string data = exportCb_();
            std::ofstream f(path);
            if (f) f << data;
        }
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}
```

**`onImport()`:**

```cpp
void SettingsDialog::onImport() {
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Import Preset",
        GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
        nullptr);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (path && importCb_)
            importCb_(std::string(path));
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}
```

**Port dropdown population:**

```cpp
void SettingsDialog::populatePorts(
        const std::vector<JackPortEnum::PortInfo>& capture,
        const std::vector<JackPortEnum::PortInfo>& playback,
        const AppSettings& s) {

    auto fill = [](GtkWidget* drop,
                   const std::vector<JackPortEnum::PortInfo>& ports,
                   const std::string& saved) {
        // Clear existing entries:
        GtkTreeModel* m = gtk_combo_box_get_model(GTK_COMBO_BOX(drop));
        gtk_list_store_clear(GTK_LIST_STORE(m));

        int sel = 0, i = 0;
        for (const auto& p : ports) {
            gtk_combo_box_text_append_text(
                GTK_COMBO_BOX_TEXT(drop), p.friendlyName.c_str());
            if (p.id == saved) sel = i;
            ++i;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(drop), sel);
    };

    fill(capL_, capture,  s.capturePort);
    fill(capR_, capture,  s.capturePort2);
    fill(pbL_,  playback, s.playbackPort);
    fill(pbR_,  playback, s.playbackPort2);
}
```

---

## 5. Signal & Callback Strategy

### GTK2 signal connection patterns

All signals in this codebase use `g_signal_connect_swapped` with stateless positive-lambda casts. This is identical to the GTK3 and GTK4 patterns:

```cpp
// Pattern — swapped callback with 'this' as user_data:
g_signal_connect_swapped(widget, "signal-name",
    G_CALLBACK(+[](MyClass* self) { self->onSomething(); }), this);
```

For callbacks that pass additional arguments (e.g. `row-activated` on `GtkTreeView`):

```cpp
// Non-swapped with explicit parameters:
g_signal_connect(treeView_, "row-activated",
    G_CALLBACK(+[](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*,
                   gpointer data) {
        static_cast<PluginDialog*>(data)->onConfirm();
    }), this);
```

### Signal suppression pattern

`ControlBar` and `PresetBar` use `suppressSignals_` (bool) to prevent re-entrant callbacks when toggling state programmatically. This pattern is identical across GTK2/3/4:

```cpp
void ControlBar::setPowerState(bool on) {
    suppressSignals_ = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(powerToggle_), on);
    suppressSignals_ = false;
}

void ControlBar::onPowerToggled() {
    if (suppressSignals_) return;
    bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(powerToggle_));
    if (powerCb_) powerCb_(on);
}
```

---

## 6. Threading Model

Unchanged from the GTK4 design (§6). JACK runs in a real-time thread; all GTK calls happen on the main thread. Cross-thread communication from JACK to GTK uses `g_idle_add` exclusively. `g_idle_add` is available in GTK2 (it is part of GLib, not GTK).

The `pollEngineState` 200 ms timer uses `g_timeout_add` with `G_SOURCE_CONTINUE` — identical API in GTK2.

```cpp
// GTK2 timer (identical to GTK4):
timerId_ = g_timeout_add(200,
    +[](gpointer data) -> gboolean {
        static_cast<MainWindow*>(data)->pollEngineState();
        return G_SOURCE_CONTINUE;
    }, this);
```

---

## 7. Styling Strategy

GTK2 does not have the GTK3/GTK4 CSS theming system. The recommended approach is a two-level strategy:

### Level 1 — GTK RC String (global)

Apply named widget styles via `gtk_rc_parse_string()` in `MainWindow::loadStyle()`. This requires assigning widget names with `gtk_widget_set_name()`.

```
GTK RC name pattern:  widget "*.delete-btn"  ← matches by widget name
GTK CSS class:        .destructive-action     ← GTK3/4 equivalent
```

### Level 2 — Per-widget `gtk_widget_modify_*` (local overrides)

For adjustments not achievable via RC strings (e.g. dynamic colour change at runtime):

```cpp
// Dynamic red highlight on xrun overflow:
GdkColor red   = { 0, 0xcccc, 0x0, 0x0 };
GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
gtk_widget_modify_bg(xrunLabel_, GTK_STATE_NORMAL, &red);
gtk_widget_modify_fg(xrunLabel_, GTK_STATE_NORMAL, &white);
```

### Theme compatibility

GTK2 apps render using the active GTK2 theme (Clearlooks, Oxygen, Adwaita-legacy, etc.). The RC string overrides integrate with the theme engine rather than replacing it. This gives a more native appearance than hardcoded colours under GTK3/GTK4 CSS.

---

## 8. Testing Checklist

### Build

- [ ] `cmake --preset linux-gtk2 && cmake --build build-linux-gtk2` succeeds with no warnings at `-Wall -Wextra`.
- [ ] Binary links against `gtk+-2.0`, not `gtk+-3.0` or `gtk+-4.0`.
- [ ] `ldd opiqo-gtk2` shows `libgtk-x11-2.0.so`.

### Window & Layout

- [ ] Window opens at 960×680 with title "Opiqo".
- [ ] Menu bar present with "File → Quit" and "View → Settings…, About".
- [ ] 2×2 slot grid fills available space (GtkTable homogeneous).
- [ ] PresetBar shows preset dropdown, name entry, Load/Save/Delete.
- [ ] ControlBar shows all controls in correct left-to-right order.

### ControlBar

- [ ] Power toggle starts/stops JACK audio.
- [ ] Gain slider sets output gain (0.0–2.0).
- [ ] Format dropdown cycles WAV/MP3/OGG/OPUS/FLAC.
- [ ] Quality combo hidden for WAV and FLAC; visible for MP3/OGG/OPUS.
- [ ] Record toggle starts/stops file recording.
- [ ] xrun label increments on JACK xruns.
- [ ] Status label reflects engine state messages.

### PresetBar

- [ ] Dropdown populated from `opiqo_named_presets.json`.
- [ ] Selecting a preset copies its name to the name entry.
- [ ] Load applies the selected preset.
- [ ] Save upserts an entry under the entered name.
- [ ] Delete removes the selected entry and refreshes the dropdown.

### Plugin Slots

- [ ] Four slots shown; all start with "Slot N" label and insensitive Bypass/Remove.
- [ ] "+ Add" disabled when engine not running (error dialog shown).
- [ ] After adding plugin: name label updated, Bypass/Remove enabled, parameter controls visible.
- [ ] After removing plugin: slot resets to empty state.
- [ ] Bypass toggle suppresses audio processing for the slot.
- [ ] Parameter sliders/checkboxes/combos update engine values in real time.
- [ ] "Browse…" button for `AtomFilePath` ports opens file chooser correctly.

### Plugin Dialog

- [ ] Dialog opens with full plugin list.
- [ ] Typing in search box filters by name and URI.
- [ ] Double-clicking or clicking "Add Plugin" loads the selected plugin.
- [ ] "Cancel" and window close dismiss without loading.

### Settings Dialog

- [ ] Opens on first Settings click; reuses same instance thereafter.
- [ ] Audio tab: four port dropdowns populated with live JACK ports.
- [ ] Apply reconnects JACK to newly selected ports.
- [ ] "Delete Plugin Cache" removes the cache file.
- [ ] Presets tab: Export saves JSON to chosen file.
- [ ] Presets tab: Import loads JSON and rebuilds all four slots.

### Stability

- [ ] No use-after-free when PluginDialog is cancelled mid-search.
- [ ] No GTK assertions ("CRITICAL" / "WARNING") in `stderr` during normal use.
- [ ] Engine error (JACK disconnect) resets power/record UI correctly.
- [ ] App exits cleanly on window close (no zombie threads).

---

## Appendix: GTK2-Specific Pitfalls

| Pitfall | Description | Fix |
|---|---|---|
| `gtk_scrolled_window_add_with_viewport` | Required for non-native-scrolling children (e.g. `GtkVBox`) | Replace `gtk_container_add` on scrolled windows with `gtk_scrolled_window_add_with_viewport` for box/label children |
| `GtkComboBoxEntry` deprecated field in GTK2.24 | `gtk_combo_box_entry_new_text` is actually in `<gtk/gtkcomboboxentry.h>` which may emit #include warnings | Use `gtk_combo_box_text_new_with_entry()` if targeting GTK 2.24+; include `<gtk/gtkcomboboxtext.h>` |
| `gtk_widget_show` required | GTK2 widgets are hidden by default; `gtk_widget_show_all` in `main_linux.cpp` covers most cases, but dynamically-created widgets (e.g. ParameterPanel rows) must be `gtk_widget_show`-ed after creation | Call `gtk_widget_show_all(row)` after appending each row in `ParameterPanel::build()` |
| `GtkHBox`/`GtkVBox` deprecation warning | If building against a GTK2 version close to 2.24, some compilers show deprecation hints for these types | Suppress with `-Wno-deprecated-declarations` or use `gtk_box_new()` only if GTK ≥ 2.16 (where it was introduced) |
| No `g_application_*` / `GtkApplication` | GTK2 has no `GtkApplication`; `gtk_init` + `gtk_main` must be used | See §4.2 |
| `GTK_STOCK_*` vs button labels | GTK2 uses stock IDs (`GTK_STOCK_OPEN`) for system-localised icons+labels; GTK3 deprecated these; GTK4 removed them | Use `GTK_STOCK_*` in GTK2 builds; use string labels in GTK3/GTK4 builds; wrap in `#ifdef` if sharing source |
| `gtk_label_set_xalign` absent | Not present in GTK2 | Use `gtk_misc_set_alignment(GTK_MISC(lbl), 0.0f, 0.5f)` |
| `gtk_separator_new(orient)` absent | Not present in GTK2 | Use `gtk_hseparator_new()` / `gtk_vseparator_new()` |
| `GtkListBox` absent | Not present before GTK 3.10 | Use `GtkTreeView` + `GtkListStore` (see §4.8) |
| `GtkHeaderBar` absent | Not present in GTK2 | Replace with `GtkMenuBar` (see §3.7) |
| `GtkCssProvider` absent | Not present in GTK2 | Replace with `gtk_rc_parse_string()` (see §3.8) |
