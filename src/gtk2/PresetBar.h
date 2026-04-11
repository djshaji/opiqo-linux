// src/gtk2/PresetBar.h (GTK2 port)
// Horizontal bar for named preset management.

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
    GtkWidget* presetDrop_ = nullptr;  // GtkComboBoxText with entry (GTK 2.24+)
    GtkWidget* nameEntry_  = nullptr;
    GtkWidget* loadBtn_    = nullptr;
    GtkWidget* saveBtn_    = nullptr;
    GtkWidget* deleteBtn_  = nullptr;

    LoadCb   loadCb_;
    SaveCb   saveCb_;
    DeleteCb deleteCb_;
};
