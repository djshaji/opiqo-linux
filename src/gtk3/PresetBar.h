// src/gtk3/PresetBar.h
// Horizontal bar for named preset management: combo-box selector, name entry,
// Load / Save / Delete buttons.  Placed between the slot grid and the ControlBar.
//
// GTK3 port — differences from the GTK4 version:
//   - GtkComboBoxText (with entry) replaces GtkDropDown + GtkStringList.
//   - gtk_box_pack_start replaces gtk_box_append.
//   - gtk_entry_get_text / gtk_entry_set_text replace gtk_editable_get/set_text.
//
// Porting notes (GTK2 / Xlib / ncurses):
//   - GTK2: same as GTK3; use gtk_combo_box_new_text() (deprecated but available).
//   - Xlib / ncurses: implement equivalent list + text-entry widgets.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <gtk/gtk.h>

class PresetBar {
public:
    using LoadCb   = std::function<void()>;
    using SaveCb   = std::function<void(const std::string& name)>;
    using DeleteCb = std::function<void()>;

    explicit PresetBar(GtkWidget* parent_box);
    ~PresetBar() = default;

    GtkWidget* widget() const { return bar_; }

    void setLoadCallback  (LoadCb   cb) { loadCb_   = std::move(cb); }
    void setSaveCallback  (SaveCb   cb) { saveCb_   = std::move(cb); }
    void setDeleteCallback(DeleteCb cb) { deleteCb_ = std::move(cb); }

    void setPresetNames(const std::vector<std::string>& names);
    void setCurrentName(const std::string& name);
    std::string getCurrentName() const;
    int getSelectedIndex() const;

private:
    void buildWidgets(GtkWidget* parent_box);
    void onDropdownChanged();

    GtkWidget* bar_        = nullptr;
    GtkWidget* presetDrop_ = nullptr;  // GtkComboBoxText with entry
    GtkWidget* nameEntry_  = nullptr;
    GtkWidget* loadBtn_    = nullptr;
    GtkWidget* saveBtn_    = nullptr;
    GtkWidget* deleteBtn_  = nullptr;

    LoadCb   loadCb_;
    SaveCb   saveCb_;
    DeleteCb deleteCb_;
};
