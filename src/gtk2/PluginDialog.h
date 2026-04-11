// src/gtk2/PluginDialog.h (GTK2 port)
// Modal plugin browser dialog — uses GtkTreeView + GtkListStore
// because GtkListBox is not available in GTK2.

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

    // GTK2 list store column indices
    enum {
        COL_NAME = 0,
        COL_SUB  = 1,
        COL_URI  = 2,
        N_COLS   = 3
    };

    void buildWidgets();
    void rebuildList(const std::string& filter);
    void onSearchChanged();
    void onConfirm();
    void onCancel();

    GtkWindow* parent_   = nullptr;
    GtkWidget* dialog_   = nullptr;
    GtkWidget* search_   = nullptr;   // plain GtkEntry (GTK2 has no GtkSearchEntry)
    GtkWidget* treeView_ = nullptr;   // GtkTreeView replacing GtkListBox
    GtkListStore* store_ = nullptr;   // backing model

    std::vector<PluginEntry> allPlugins_;
    ConfirmCb                confirmCb_;
};
