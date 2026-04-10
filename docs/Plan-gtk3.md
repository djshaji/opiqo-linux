# Opiqo — GTK3 Implementation Plan

> Based on: `design/gtk4.md` (reference implementation)  
> Target toolkit: GTK 3.x (tested against GTK 3.22+)  
> Domain layer (`LiveEffectEngine`, `AudioEngine`, `FileWriter`, `LV2Plugin`, `AppSettings`, `JackPortEnum`) is **shared verbatim** with the GTK4 build.

---

## Table of Contents

1. [Scope & Goals](#1-scope--goals)
2. [Source Layout](#2-source-layout)
3. [API Mapping: GTK4 → GTK3](#3-api-mapping-gtk4--gtk3)
4. [Implementation Tasks](#4-implementation-tasks)
   - 4.1 [Build System](#41-build-system)
   - 4.2 [main_linux.cpp](#42-main_linuxcpp)
   - 4.3 [MainWindow](#43-mainwindow)
   - 4.4 [ControlBar](#44-controlbar)
   - 4.5 [PluginSlot](#45-pluginslot)
   - 4.6 [ParameterPanel](#46-parameterpanel)
   - 4.7 [PluginDialog](#47-plugindialog)
   - 4.8 [SettingsDialog](#48-settingsdialog)
5. [Signal & Callback Strategy](#5-signal--callback-strategy)
6. [Threading Model](#6-threading-model)
7. [Deprecated GTK3 APIs to Avoid](#7-deprecated-gtk3-apis-to-avoid)
8. [CSS / Styling](#8-css--styling)
9. [Testing Checklist](#9-testing-checklist)

---

## 1. Scope & Goals

Produce a pixel-for-pixel functional equivalent of the GTK4 UI using only GTK3 APIs, with **no changes to the domain layer**. The user-visible feature set is identical:

- 2×2 grid of plugin slots, each with Add / Bypass / Remove controls and a scrollable parameter panel.
- Bottom control bar: Power toggle, Gain slider, Format/Quality dropdowns, Record toggle, xrun counter, status label.
- Settings dialog: JACK port selection (two capture, two playback), sample rate / block size display, plugin cache invalidation, and preset export/import.
- About dialog.
- Plugin browser dialog with live search.

---

## 2. Source Layout

Create a new subdirectory `src/gtk3/` mirroring `src/gtk4/`. All filenames and class names are identical; only the includes and GTK API calls differ.

```
src/gtk3/
    AppSettings.h/.cpp        ← symlink or copy from gtk4/ (unchanged)
    AudioEngine.h/.cpp        ← symlink or copy from gtk4/ (unchanged)
    JackPortEnum.h/.cpp       ← symlink or copy from gtk4/ (unchanged)
    version.h                 ← symlink or copy from gtk4/ (unchanged)
    MainWindow.h/.cpp         ← port from gtk4/MainWindow
    ControlBar.h/.cpp         ← port from gtk4/ControlBar
    PluginSlot.h/.cpp         ← port from gtk4/PluginSlot
    ParameterPanel.h/.cpp     ← port from gtk4/ParameterPanel
    PluginDialog.h/.cpp       ← port from gtk4/PluginDialog
    SettingsDialog.h/.cpp     ← port from gtk4/SettingsDialog
```

Files that do not contain any GTK calls (`AppSettings`, `AudioEngine`, `JackPortEnum`, `version.h`) should be symlinked rather than copied to avoid divergence.

---

## 3. API Mapping: GTK4 → GTK3

### Container / Layout Widgets

| GTK4 | GTK3 replacement | Notes |
|---|---|---|
| `gtk_box_new(GTK_ORIENTATION_HORIZONTAL, n)` | Same | Identical API |
| `gtk_box_new(GTK_ORIENTATION_VERTICAL, n)` | Same | Identical API |
| `gtk_box_append(box, child)` | `gtk_box_pack_start(box, child, expand, fill, 0)` | GTK3 uses `pack_start`/`pack_end` |
| `gtk_grid_new()` | Same | Identical API |
| `gtk_grid_attach(...)` | Same | Identical API |
| `gtk_frame_new(label)` | Same | Identical API |
| `gtk_scrolled_window_new()` | `gtk_scrolled_window_new(NULL, NULL)` | GTK3 takes hadjustment/vadjustment args |
| `gtk_scrolled_window_set_child(sw, child)` | `gtk_container_add(GTK_CONTAINER(sw), child)` | GTK3 uses `gtk_container_add` |
| `gtk_window_set_child(win, child)` | `gtk_container_add(GTK_CONTAINER(win), child)` | GTK3 uses `gtk_container_add` |
| `gtk_frame_set_child(frame, child)` | `gtk_container_add(GTK_CONTAINER(frame), child)` | GTK3 uses `gtk_container_add` |
| `GtkHeaderBar` | `GtkHeaderBar` | Available in GTK 3.10+; same basic API |

### Buttons & Input

| GTK4 | GTK3 replacement | Notes |
|---|---|---|
| `gtk_button_new_with_label(label)` | Same | Identical |
| `gtk_toggle_button_new_with_label(label)` | Same | Identical |
| `gtk_check_button_new()` | Same | Identical |
| `gtk_check_button_set_active(btn, val)` | `gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), val)` | `GtkCheckButton` is a `GtkToggleButton` in GTK3 |
| `gtk_check_button_get_active(btn)` | `gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn))` | Same pattern |
| `gtk_scale_new_with_range(...)` | Same | Identical |
| `gtk_search_entry_new()` | Same | Available since GTK 3.6 |
| `gtk_search_entry_set_placeholder_text(entry, text)` | `gtk_entry_set_placeholder_text(GTK_ENTRY(entry), text)` | `GtkSearchEntry` extends `GtkEntry` |
| `gtk_editable_get_text(entry)` | `gtk_entry_get_text(GTK_ENTRY(entry))` | GTK3 uses `GtkEntry` API directly |

### Dropdowns

| GTK4 | GTK3 replacement | Notes |
|---|---|---|
| `GtkDropDown` + `GtkStringList` | `GtkComboBoxText` | Simpler API, no factory needed |
| `gtk_drop_down_new(model, expr)` | `gtk_combo_box_text_new()` | |
| `gtk_drop_down_set_selected(dd, idx)` | `gtk_combo_box_set_active(GTK_COMBO_BOX(dd), idx)` | |
| `gtk_drop_down_get_selected(dd)` | `gtk_combo_box_get_active(GTK_COMBO_BOX(dd))` | |
| `gtk_string_list_new(items)` + `gtk_drop_down_set_model` | `gtk_combo_box_text_append_text(dd, text)` loop | |
| `notify::selected` signal | `changed` signal on `GtkComboBox` | |

### Labels

| GTK4 | GTK3 replacement |
|---|---|
| `gtk_label_new(text)` | Same |
| `gtk_label_set_text(lbl, text)` | Same |
| `gtk_label_set_xalign(lbl, f)` | `gtk_misc_set_alignment(GTK_MISC(lbl), f, 0.5f)` or `gtk_label_set_halign` |
| `gtk_label_set_wrap(lbl, TRUE)` | Same |
| `gtk_label_set_ellipsize(lbl, mode)` | Same |

### Dialogs

| GTK4 | GTK3 replacement | Notes |
|---|---|---|
| `GtkMessageDialog` (still works in GTK4) | `GtkMessageDialog` | Same API; use `gtk_dialog_run()` for simplicity |
| `GtkAboutDialog` | `GtkAboutDialog` | Identical API |
| `GtkFileDialog::open()` (async, GTK 4.10+) | `GtkFileChooserDialog` + `gtk_dialog_run()` | Synchronous; simpler for GTK3 |
| `GtkFileDialog::save()` (async, GTK 4.10+) | `GtkFileChooserDialog` with `GTK_FILE_CHOOSER_ACTION_SAVE` | Synchronous |
| `gtk_window_destroy(win)` | `gtk_widget_destroy(win)` | GTK3 uses `gtk_widget_destroy` |

### Widget Visibility & Sensitivity

| GTK4 | GTK3 replacement |
|---|---|
| `gtk_widget_set_visible(w, TRUE/FALSE)` | Same |
| `gtk_widget_show(w)` | Same |
| `gtk_widget_set_sensitive(w, TRUE/FALSE)` | Same |
| `gtk_widget_add_css_class(w, class)` | `gtk_style_context_add_class(gtk_widget_get_style_context(w), class)` |

### Window Management

| GTK4 | GTK3 replacement |
|---|---|
| `gtk_window_set_transient_for(w, parent)` | Same |
| `gtk_window_set_modal(w, TRUE)` | Same |
| `gtk_window_set_default_size(w, x, y)` | Same |
| `gtk_window_set_destroy_with_parent(w, TRUE)` | Same |
| `gtk_application_window_new(app)` | Same |

### List Widgets (PluginDialog)

| GTK4 | GTK3 replacement | Notes |
|---|---|---|
| `GtkListBox` | `GtkListBox` | Available in GTK 3.10+ |
| `gtk_list_box_append(lb, row)` | `gtk_container_add(GTK_CONTAINER(lb), row)` | GTK3 uses `gtk_container_add` |
| `gtk_list_box_remove(lb, row)` | `gtk_container_remove(GTK_CONTAINER(lb), row)` | |
| `gtk_list_box_row_get_child(row)` | Same | |
| `gtk_widget_get_first_child(widget)` | `gtk_container_get_children(GTK_CONTAINER(widget))` → first element | Returns a `GList*` |

### Notebook

| GTK4 | GTK3 |
|---|---|
| `gtk_notebook_new()` | Same |
| `gtk_notebook_append_page(nb, child, label)` | Same |

---

## 4. Implementation Tasks

### 4.1 Build System

**In `CMakeLists.txt`**, add a new configure preset and build target for GTK3:

```cmake
# In CMakeLists.txt, alongside the existing gtk4 block:
if(OPIQO_TARGET_PLATFORM STREQUAL "linux-gtk3")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    set(GTK3_SRCS
        src/main_linux.cpp
        src/gtk3/MainWindow.cpp
        src/gtk3/ControlBar.cpp
        src/gtk3/PluginSlot.cpp
        src/gtk3/ParameterPanel.cpp
        src/gtk3/PluginDialog.cpp
        src/gtk3/SettingsDialog.cpp
        src/gtk3/AppSettings.cpp
        src/gtk3/AudioEngine.cpp
        src/gtk3/JackPortEnum.cpp
        # shared domain sources:
        src/LiveEffectEngine.cpp
        src/FileWriter.cpp
        src/LockFreeQueue.cpp
    )
    
    add_executable(opiqo-gtk3 ${GTK3_SRCS})
    target_include_directories(opiqo-gtk3 PRIVATE src src/gtk3
        ${GTK3_INCLUDE_DIRS})
    target_link_libraries(opiqo-gtk3
        ${GTK3_LIBRARIES} jack lilv-0 sndfile opus opusenc mp3lame FLAC
        -ldl -lpthread)
endif()
```

**In `CMakePresets.json`**, add:

```json
{
    "name": "linux-gtk3",
    "displayName": "Linux GTK3 (JACK)",
    "binaryDir": "${sourceDir}/build-linux-gtk3",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "OPIQO_TARGET_PLATFORM": "linux-gtk3"
    }
}
```

---

### 4.2 main_linux.cpp

The GTK3 entry point is identical to the GTK4 version — `GtkApplication` API is the same. No changes required if `main_linux.cpp` lives in `src/` and only includes `src/gtk3/MainWindow.h` via the include path.

The `activate` callback pattern is unchanged:

```cpp
static void activate(GtkApplication* app, gpointer) {
    auto* win = new MainWindow(app);
    gtk_widget_show_all(win->window());   // GTK3: show_all instead of gtk_widget_show
}
```

> **GTK3 Note:** Use `gtk_widget_show_all()` on the top-level window to recursively show all children. In GTK4 widgets are visible by default; in GTK3 they are hidden by default and must be shown explicitly.

---

### 4.3 MainWindow

**Constructor changes:**

- `buildWidgets()` implementation differences (see below).
- `gtk_widget_show_all(window_)` instead of `gtk_widget_show(window_)` at the end of construction.

**`buildWidgets()` changes:**

```cpp
// GTK4:
gtk_window_set_child(GTK_WINDOW(window_), root);

// GTK3:
gtk_container_add(GTK_CONTAINER(window_), root);
```

```cpp
// GTK4:
gtk_box_append(GTK_BOX(root), slotGrid_);

// GTK3:
gtk_box_pack_start(GTK_BOX(root), slotGrid_, TRUE, TRUE, 0);
```

```cpp
// GTK4:
gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

// GTK3:
gtk_box_pack_start(GTK_BOX(root),
    gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
```

**`showAboutDialog()`:** No changes — `GtkAboutDialog` API is identical.

**`onAddPlugin()` alert dialog:**

```cpp
// GTK3 (replace GtkMessageDialog usage):
GtkWidget* alert = gtk_message_dialog_new(
    GTK_WINDOW(window_),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
    "Start the engine before loading plugins");
gtk_dialog_run(GTK_DIALOG(alert));   // synchronous in GTK3
gtk_widget_destroy(alert);
```

**`loadCss()`:**

```cpp
// GTK3:
GtkCssProvider* css = gtk_css_provider_new();
gtk_css_provider_load_from_data(css, kAppCss, -1, nullptr);
gtk_style_context_add_provider_for_screen(
    gdk_screen_get_default(),
    GTK_STYLE_PROVIDER(css),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
g_object_unref(css);
```

> `gtk_css_provider_load_from_string` (GTK4) → `gtk_css_provider_load_from_data` (GTK3).  
> `gdk_display_get_default()` (GTK4) → `gdk_screen_get_default()` (GTK3).

---

### 4.4 ControlBar

**`buildWidgets()` changes:**

Replace all `gtk_box_append` calls with `gtk_box_pack_start` / `gtk_box_pack_end`:

```cpp
// Expanding spacer (push xrun + status to the right):
gtk_box_pack_start(GTK_BOX(bar_), spacer, TRUE, TRUE, 0);

// Non-expanding widgets (buttons, labels, separators):
gtk_box_pack_start(GTK_BOX(bar_), powerToggle_, FALSE, FALSE, 0);
gtk_box_pack_end(GTK_BOX(bar_), statusLabel_, FALSE, FALSE, 0);
gtk_box_pack_end(GTK_BOX(bar_), xrunLabel_,   FALSE, FALSE, 0);
```

**`GtkDropDown` → `GtkComboBoxText`:**

```cpp
// Format dropdown:
formatDrop_ = gtk_combo_box_text_new();
const char* formats[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
for (const char* f : formats)
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(formatDrop_), f);
gtk_combo_box_set_active(GTK_COMBO_BOX(formatDrop_), 0);

g_signal_connect_swapped(formatDrop_, "changed",
    G_CALLBACK(+[](ControlBar* self) { self->onFormatChanged(); }), this);

// Quality dropdown (same pattern, entries "0 (low)" .. "9 (high)"):
qualityDrop_ = gtk_combo_box_text_new();
// ... append entries ...
gtk_combo_box_set_active(GTK_COMBO_BOX(qualityDrop_), 5);
```

**Reading selected item:**

```cpp
// GTK4:
const int fmt = gtk_drop_down_get_selected(GTK_DROP_DOWN(formatDrop_));

// GTK3:
const int fmt = gtk_combo_box_get_active(GTK_COMBO_BOX(formatDrop_));
```

**CSS classes:**

```cpp
// GTK4:
gtk_widget_add_css_class(xrunLabel_, "xrun-label");

// GTK3:
gtk_style_context_add_class(
    gtk_widget_get_style_context(xrunLabel_), "xrun-label");
```

**`onFormatChanged()` visibility:**

```cpp
// GTK3 — same logic, different visibility call:
gtk_widget_set_visible(qualityBox_, lossy);
// (gtk_widget_set_visible is the same in GTK3)
```

---

### 4.5 PluginSlot

**`buildWidgets()` changes:**

```cpp
// GTK4:
gtk_frame_set_child(GTK_FRAME(frame_), outerBox);

// GTK3:
gtk_container_add(GTK_CONTAINER(frame_), outerBox);
```

```cpp
// All gtk_box_append → gtk_box_pack_start:
gtk_box_pack_start(GTK_BOX(outerBox), headerBox_, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(outerBox),
    gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(outerBox), paramPanel_->widget(), TRUE, TRUE, 0);
```

```cpp
// Header box children:
gtk_box_pack_start(GTK_BOX(headerBox_), nameLabel_,    TRUE,  TRUE,  0);
gtk_box_pack_start(GTK_BOX(headerBox_), addButton_,    FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(headerBox_), bypassButton_, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(headerBox_), deleteButton_, FALSE, FALSE, 0);
```

**CSS class on delete button:**

```cpp
// GTK3:
gtk_style_context_add_class(
    gtk_widget_get_style_context(deleteButton_), "destructive-action");
```

**`onPluginAdded` / `onPluginCleared`:** No logical changes; `gtk_label_set_text`, `gtk_widget_set_sensitive`, and `gtk_toggle_button_set_active` are identical in GTK3.

---

### 4.6 ParameterPanel

**Constructor:**

```cpp
// GTK3:
scroll_ = gtk_scrolled_window_new(nullptr, nullptr);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_),
    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
gtk_widget_set_vexpand(scroll_, TRUE);
gtk_widget_set_hexpand(scroll_, TRUE);

box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
gtk_container_add(GTK_CONTAINER(scroll_), box_);
```

**`build()` — row packing:**

```cpp
// GTK3:
gtk_box_pack_start(GTK_BOX(row), lbl,  FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(row), ctrl, TRUE,  TRUE,  0);
gtk_box_pack_start(GTK_BOX(box_), row, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(box_),
    gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
```

**`clear()` — removing children:**

```cpp
// GTK4:
GtkWidget* child;
while ((child = gtk_widget_get_first_child(box_)) != nullptr)
    gtk_box_remove(GTK_BOX(box_), child);

// GTK3:
GList* children = gtk_container_get_children(GTK_CONTAINER(box_));
for (GList* l = children; l != nullptr; l = l->next)
    gtk_container_remove(GTK_CONTAINER(box_), GTK_WIDGET(l->data));
g_list_free(children);
```

**`GtkDropDown` → `GtkComboBoxText` for enum ports:**

```cpp
// GTK3 enum control:
GtkWidget* ctrl = gtk_combo_box_text_new();
for (const auto& sp : port.scalePoints)
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctrl),
                                   sp.second.c_str());
// Set initial value:
gtk_combo_box_set_active(GTK_COMBO_BOX(ctrl), static_cast<int>(sel));

g_signal_connect_data(ctrl, "changed", ...);
```

**File browse (`AtomFilePath`) — synchronous `GtkFileChooserDialog`:**

```cpp
// GTK3 (replace async GtkFileDialog):
GtkWidget* dialog = gtk_file_chooser_dialog_new(
    "Open file",
    GTK_WINDOW(self->parent_),
    GTK_FILE_CHOOSER_ACTION_OPEN,
    "_Cancel", GTK_RESPONSE_CANCEL,
    "_Open",   GTK_RESPONSE_ACCEPT,
    nullptr);

if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if (path && cd->panel->fileCb_)
        cd->panel->fileCb_(cd->writableUri, path);
    g_free(path);
}
gtk_widget_destroy(dialog);
```

> This runs synchronously (blocks the main loop during the dialog). This is the standard GTK3 pattern. In GTK4, `GtkFileDialog::open()` is async; in GTK3, `gtk_dialog_run()` is the idiomatic approach.

---

### 4.7 PluginDialog

**Widget structure:** Replace `GtkListBox` `append`/`remove` calls and use `gtk_container_add` / `gtk_container_remove`. Everything else (`GtkListBox`, `GtkListBoxRow`, `GtkSearchEntry`) is available identically in GTK3 10+.

**Key changes:**

```cpp
// Appending a row:
// GTK4: gtk_list_box_append(GTK_LIST_BOX(listBox_), rowBox);
// GTK3: gtk_container_add(GTK_CONTAINER(listBox_), rowBox);

// Removing all rows:
// GTK4:
//   GtkWidget* child;
//   while ((child = gtk_widget_get_first_child(listBox_)) != nullptr)
//       gtk_list_box_remove(GTK_LIST_BOX(listBox_), child);
// GTK3:
GList* rows = gtk_container_get_children(GTK_CONTAINER(listBox_));
for (GList* l = rows; l; l = l->next)
    gtk_container_remove(GTK_CONTAINER(listBox_), GTK_WIDGET(l->data));
g_list_free(rows);
```

**Search entry placeholder:**

```cpp
// GTK3:
gtk_entry_set_placeholder_text(GTK_ENTRY(search_), "Filter plugins…");
```

**Search change signal:**

```cpp
// GTK3: "search-changed" signal is available on GtkSearchEntry since GTK 3.16.
// g_signal_connect_swapped stays the same.
```

**editable text:**

```cpp
// GTK4: gtk_editable_get_text(GTK_EDITABLE(search_))
// GTK3: gtk_entry_get_text(GTK_ENTRY(search_))
```

**Destroying the window:**

```cpp
// GTK4: gtk_window_destroy(GTK_WINDOW(dialog_));
// GTK3: gtk_widget_destroy(dialog_);
```

**`onConfirm()` — show_all after construction:**

In GTK3 call `gtk_widget_show_all(dialog_)` in `buildWidgets()` or `show()` so all child widgets are visible.

**Button CSS class:**

```cpp
// GTK3:
gtk_style_context_add_class(
    gtk_widget_get_style_context(addBtn), "suggested-action");
```

---

### 4.8 SettingsDialog

**Window setup:** Replace `gtk_window_set_child` with `gtk_container_add`. Call `gtk_widget_show_all` at the end of `show()`.

**`buildAudioTab()` changes:**

- Port dropdowns: use `GtkComboBoxText` instead of `GtkDropDown`.
- Helper `populatePortDropDown`: append items with `gtk_combo_box_text_append_text`.
- `selectedPort()`: use `gtk_combo_box_get_active` to get index.

```cpp
void SettingsDialog::populatePortDropDown(GtkWidget* dd,
                                          const std::vector<PortInfo>& ports,
                                          const std::string& selected) {
    for (const auto& p : ports)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dd),
                                       p.friendlyName.c_str());
    guint sel = 0;
    for (guint i = 0; i < ports.size(); ++i)
        if (ports[i].id == selected) { sel = i; break; }
    gtk_combo_box_set_active(GTK_COMBO_BOX(dd), static_cast<int>(sel));
}

std::string SettingsDialog::selectedPort(GtkWidget* dd,
                                          const std::vector<PortInfo>& ports) const {
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(dd));
    return (idx >= 0 && idx < (int)ports.size()) ? ports[idx].id : "";
}
```

**`buildPresetsTab()` — file chooser dialogs (synchronous):**

```cpp
// Export:
void SettingsDialog::onExport() {
    if (!exportCb_) return;
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Export Preset", GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save",   GTK_RESPONSE_ACCEPT, nullptr);
    gtk_file_chooser_set_current_name(
        GTK_FILE_CHOOSER(dlg), "opiqo-preset.json");
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (path) {
            std::string json = exportCb_();
            std::ofstream f(path);
            if (f.is_open()) f << json;
            g_free(path);
        }
    }
    gtk_widget_destroy(dlg);
}

// Import:
void SettingsDialog::onImport() {
    if (!importCb_) return;
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Import Preset", GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT, nullptr);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (path) { importCb_(path); g_free(path); }
    }
    gtk_widget_destroy(dlg);
}
```

**Apply button — CSS class:**

```cpp
gtk_style_context_add_class(
    gtk_widget_get_style_context(applyBtn), "suggested-action");
```

---

## 5. Signal & Callback Strategy

All signal connections use the same `g_signal_connect_swapped` + stateless lambda pattern from GTK4. This pattern compiles and works identically in GTK3.

The only signal name changes are:

| GTK4 signal | GTK3 signal | Widget |
|---|---|---|
| `notify::selected` | `changed` | `GtkComboBoxText` (was `GtkDropDown`) |
| `search-changed` | `search-changed` | `GtkSearchEntry` (unchanged) |

All other signals (`clicked`, `toggled`, `value-changed`, `row-activated`) are identical.

The full callback chain (ParameterPanel → PluginSlot → MainWindow → domain) is **unchanged** — only the leaf signal names at the widget level differ for dropdowns.

---

## 6. Threading Model

Identical to the GTK4 implementation:

- All UI callbacks run on the GTK main thread.
- JACK RT thread communicates back to main thread via `g_idle_add` only.
- `std::atomic` protects shared state (`state_`, `sampleRate_`, `blockSize_`, `xrunCount_`).

No changes required in `AudioEngine` or `JackPortEnum`.

---

## 7. Deprecated GTK3 APIs to Avoid

The following GTK3 APIs are deprecated and should not be used even though they exist:

| Deprecated | Use instead |
|---|---|
| `GtkVBox` / `GtkHBox` | `gtk_box_new(GTK_ORIENTATION_VERTICAL/HORIZONTAL, n)` |
| `GtkTable` | `GtkGrid` |
| `gtk_misc_set_alignment` | `gtk_widget_set_halign` / `gtk_label_set_xalign` |
| `gtk_button_new_from_stock` | `gtk_button_new_with_label` or icon-name variant |
| `gtk_image_new_from_stock` | `gtk_image_new_from_icon_name` |
| `gtk_dialog_run` in GTK 3.24+ | Still acceptable in GTK3 context, but be aware it is removed in GTK4 |
| `GtkComboBox` with tree model | `GtkComboBoxText` for simple string lists |

---

## 8. CSS / Styling

The same embedded CSS string (`kAppCss`) from the GTK4 implementation works in GTK3 without modification. GTK3 CSS is a subset of GTK4 CSS; the class names and basic selectors used in Opiqo (`.plugin-slot`, `.slot-header`, `.slot-name`, `.status-label`, `.xrun-label`, `.plugin-name`) are supported.

The only change is the provider registration call:

```cpp
// GTK4:
gtk_style_context_add_provider_for_display(
    gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

// GTK3:
gtk_style_context_add_provider_for_screen(
    gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
```

And the load function:

```cpp
// GTK4: gtk_css_provider_load_from_string(css, kAppCss);
// GTK3: gtk_css_provider_load_from_data(css, kAppCss, -1, nullptr);
```

---

## 9. Testing Checklist

Before the GTK3 port is considered complete, verify each item:

### Build
- [ ] `cmake --preset linux-gtk3 && cmake --build build-linux-gtk3` completes without warnings.
- [ ] No GTK4-only symbols referenced (verify with `nm` or `ldd`).

### Startup
- [ ] Application window opens at 960×680.
- [ ] Plugin cache loads (or scans and saves on first run).
- [ ] All four plugin slots are visible in a 2×2 grid.
- [ ] Control bar is visible at the bottom.

### Plugin Management
- [ ] Clicking "+ Add" when engine is stopped shows `GtkMessageDialog` warning.
- [ ] Clicking "+ Add" when engine is running opens `PluginDialog`.
- [ ] Search box filters plugin list (case-insensitive, name and URI).
- [ ] Double-clicking a plugin (row-activated) loads it.
- [ ] Slot label updates to plugin name after load.
- [ ] Bypass and Remove buttons become sensitive after load.
- [ ] Bypass toggle sends `setPluginEnabled` to engine.
- [ ] Remove clears slot UI and calls `deletePlugin`.

### Parameter Panel
- [ ] Float ports render as `GtkScale` with correct min/max.
- [ ] Enum ports render as `GtkComboBoxText` with correct entries.
- [ ] Toggle ports render as `GtkCheckButton`.
- [ ] Trigger ports render as "Fire" button.
- [ ] AtomFilePath ports show "Browse…" button that opens `GtkFileChooserDialog`.
- [ ] Adjusting a control sends the value to the engine in real time.

### Audio Engine
- [ ] Power toggle starts JACK client.
- [ ] Status bar shows sample rate and block size after start.
- [ ] Gain slider updates engine gain.
- [ ] Xrun counter increments on buffer underruns.
- [ ] Engine error resets power toggle and shows status message.

### Recording
- [ ] Format dropdown shows WAV/MP3/OGG/OPUS/FLAC.
- [ ] Quality dropdown is hidden for WAV and FLAC, visible for MP3/OGG/OPUS.
- [ ] Record toggle is blocked when engine is not running.
- [ ] Recording creates a timestamped file in the Music directory.
- [ ] Stopping recording closes the file cleanly.

### Settings Dialog
- [ ] Opens as a modal window.
- [ ] JACK capture and playback ports are populated.
- [ ] Apply restarts engine with new ports if it was running.
- [ ] "Delete Plugin Cache" button removes the cache file.
- [ ] Export Preset saves valid JSON via `GtkFileChooserDialog`.
- [ ] Import Preset restores slot state from JSON file.

### About Dialog
- [ ] Shows version `0.8.0`, codename "Jag-Stang", author, website link.

### Settings Persistence
- [ ] Settings saved to `~/.config/opiqo/settings.json` on exit.
- [ ] Settings restored on next launch.
