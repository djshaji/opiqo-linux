// src/gtk3/PluginDialog.h (GTK3 port)
// Modal plugin browser dialog.

#pragma once

#include <functional>
#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "json.hpp"

class PluginDialog {
public:
    using ConfirmCb = std::function<void(const std::string& uri)>;

    PluginDialog(GtkWindow* parent, const nlohmann::json& plugins);
    ~PluginDialog();

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

    GtkWindow* parent_  = nullptr;
    GtkWidget* dialog_  = nullptr;
    GtkWidget* search_  = nullptr;
    GtkWidget* listBox_ = nullptr;

    std::vector<PluginEntry> allPlugins_;
    ConfirmCb                confirmCb_;
};
