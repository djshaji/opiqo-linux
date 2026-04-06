// src/linux/PluginDialog.h
// Modal plugin browser dialog backed by LiveEffectEngine::getAvailablePlugins().
// Provides a searchable list of installed LV2 plugins.

#pragma once

#include <functional>
#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "json.hpp"

class PluginDialog {
public:
    // Fired when the user confirms a selection; uri is the LV2 plugin URI.
    using ConfirmCb = std::function<void(const std::string& uri)>;

    // parent: the main window (for modal parenting).
    // plugins: the JSON returned by LiveEffectEngine::getAvailablePlugins().
    PluginDialog(GtkWindow* parent, const nlohmann::json& plugins);
    ~PluginDialog();

    // Show the dialog and call cb when the user confirms.
    // Non-blocking; the dialog is self-managed.
    void show(ConfirmCb cb);

private:
    struct PluginEntry {
        std::string uri;
        std::string name;
        std::string author;
    };

    void buildWidgets();
    void rebuildList(const std::string& filter);
    void onSearchChanged();
    void onConfirm();
    void onCancel();

    GtkWindow*           parent_  = nullptr;
    GtkWidget*           dialog_  = nullptr;
    GtkWidget*           search_  = nullptr;
    GtkWidget*           listBox_ = nullptr;

    std::vector<PluginEntry> allPlugins_;   // full, sorted list
    ConfirmCb                confirmCb_;
};
