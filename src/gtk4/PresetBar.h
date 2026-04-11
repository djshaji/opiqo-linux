// src/gtk4/PresetBar.h
// Horizontal bar for named preset management: dropdown selector, name entry,
// Load / Save / Delete buttons.  Placed between the slot grid and the ControlBar.
//
// Porting notes (GTK3 / GTK2 / Xlib / ncurses):
//   - Replace GtkDropDown + GtkStringList with GtkComboBoxText (GTK3/2).
//   - Replace gtk_box_append with gtk_box_pack_start (GTK3/2).
//   - Replace gtk_editable_get_text with gtk_entry_get_text (GTK3/2).
//   - For Xlib / ncurses: implement equivalent list + text-entry widgets.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <gtk/gtk.h>

class PresetBar {
public:
    // Called when the user clicks "Load"; the host reads getSelectedIndex() to
    // identify which named preset to apply.
    using LoadCb   = std::function<void()>;
    // Called when the user clicks "Save"; receives the name from the name entry.
    using SaveCb   = std::function<void(const std::string& name)>;
    // Called when the user clicks "Delete"; the host reads getSelectedIndex().
    using DeleteCb = std::function<void()>;

    explicit PresetBar(GtkWidget* parent_box);
    ~PresetBar() = default;

    GtkWidget* widget() const { return bar_; }

    void setLoadCallback  (LoadCb   cb) { loadCb_   = std::move(cb); }
    void setSaveCallback  (SaveCb   cb) { saveCb_   = std::move(cb); }
    void setDeleteCallback(DeleteCb cb) { deleteCb_ = std::move(cb); }

    // Replace the dropdown contents with the given preset names.
    void setPresetNames(const std::vector<std::string>& names);
    // Set the name entry text (e.g. when the user selects a preset).
    void setCurrentName(const std::string& name);
    // Return text currently in the name entry.
    std::string getCurrentName() const;
    // Return the selected index in the dropdown, or -1 if nothing is selected.
    int getSelectedIndex() const;

private:
    void buildWidgets(GtkWidget* parent_box);
    void onDropdownChanged();

    GtkWidget*     bar_        = nullptr;
    GtkWidget*     presetDrop_ = nullptr;
    GtkWidget*     nameEntry_  = nullptr;
    GtkWidget*     loadBtn_    = nullptr;
    GtkWidget*     saveBtn_    = nullptr;
    GtkWidget*     deleteBtn_  = nullptr;
    GtkStringList* stringList_ = nullptr;

    LoadCb   loadCb_;
    SaveCb   saveCb_;
    DeleteCb deleteCb_;
};
