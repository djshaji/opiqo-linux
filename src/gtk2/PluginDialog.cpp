// src/gtk2/PluginDialog.cpp (GTK2 port)
// Uses GtkTreeView + GtkListStore in place of GTK3's GtkListBox.

#include "PluginDialog.h"

#include <algorithm>
#include <cctype>

using json = nlohmann::json;

// ── PluginDialog ──────────────────────────────────────────────────────────────

PluginDialog::PluginDialog(GtkWindow* parent, const json& plugins) : parent_(parent) {
    for (auto it = plugins.begin(); it != plugins.end(); ++it) {
        const json& info = it.value();
        PluginEntry entry;
        entry.uri    = it.key();
        entry.name   = info.value("name",   entry.uri);
        entry.author = info.value("author", std::string(""));
        allPlugins_.push_back(std::move(entry));
    }
    std::sort(allPlugins_.begin(), allPlugins_.end(),
              [](const PluginEntry& a, const PluginEntry& b) {
                  return a.name < b.name;
              });

    buildWidgets();
}

PluginDialog::~PluginDialog() {
    if (dialog_) {
        gtk_widget_destroy(dialog_);
        dialog_ = nullptr;
    }
}

void PluginDialog::buildWidgets() {
    dialog_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog_), "Add Plugin");
    gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent_);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 540, 480);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);

    // GTK2: gtk_vbox_new instead of gtk_box_new(VERTICAL, ...)
    GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(dialog_), vbox);

    // ── Filter entry (GTK2 has no GtkSearchEntry) ─────────────────────────
    GtkWidget* searchBox = gtk_hbox_new(FALSE, 4);
    GtkWidget* searchLabel = gtk_label_new("Filter:");
    gtk_box_pack_start(GTK_BOX(searchBox), searchLabel, FALSE, FALSE, 0);

    search_ = gtk_entry_new();
    gtk_widget_set_tooltip_text(search_, "Filter plugins by name or URI");
    g_signal_connect_swapped(search_, "changed",
        G_CALLBACK(+[](PluginDialog* self) { self->onSearchChanged(); }), this);
    gtk_box_pack_start(GTK_BOX(searchBox), search_, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), searchBox, FALSE, FALSE, 0);

    // ── GtkListStore: name (text), sub (author+uri), uri (key) ────────────
    store_ = gtk_list_store_new(N_COLS,
        G_TYPE_STRING,   // COL_NAME
        G_TYPE_STRING,   // COL_SUB
        G_TYPE_STRING);  // COL_URI

    // ── GtkTreeView ───────────────────────────────────────────────────────
    treeView_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_));
    g_object_unref(store_);   // view holds its own reference

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView_), FALSE);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeView_), TRUE);

    // Column 0: plugin name
    GtkCellRenderer* nameRender = gtk_cell_renderer_text_new();
    g_object_set(nameRender, "weight", PANGO_WEIGHT_BOLD, nullptr);
    GtkTreeViewColumn* nameCol = gtk_tree_view_column_new_with_attributes(
        "Name", nameRender, "text", COL_NAME, nullptr);
    gtk_tree_view_column_set_expand(nameCol, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeView_), nameCol);

    // Column 1: author + uri (dimmed, ellipsized)
    GtkCellRenderer* subRender = gtk_cell_renderer_text_new();
    g_object_set(subRender,
        "foreground", "gray",
        "ellipsize",  PANGO_ELLIPSIZE_END,
        nullptr);
    GtkTreeViewColumn* subCol = gtk_tree_view_column_new_with_attributes(
        "Info", subRender, "text", COL_SUB, nullptr);
    gtk_tree_view_column_set_expand(subCol, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeView_), subCol);

    // Double-click or Enter activates selection
    g_signal_connect_swapped(treeView_, "row-activated",
        G_CALLBACK(+[](PluginDialog* self, GtkTreePath*, GtkTreeViewColumn*) {
            self->onConfirm();
        }), this);

    // ── Scrolled window containing tree view ──────────────────────────────
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    // GtkTreeView is a native-scrolling widget in GTK2; use gtk_container_add
    gtk_container_add(GTK_CONTAINER(scroll), treeView_);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    // Populate initially (no filter)
    rebuildList("");

    // ── Button row ────────────────────────────────────────────────────────
    GtkWidget* btnBox = gtk_hbox_new(FALSE, 6);

    // Spacer on the left
    gtk_box_pack_start(GTK_BOX(btnBox), gtk_hbox_new(FALSE, 0), TRUE, TRUE, 0);

    GtkWidget* cancelBtn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancelBtn, "clicked",
        G_CALLBACK(+[](PluginDialog* self) { self->onCancel(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), cancelBtn, FALSE, FALSE, 0);

    GtkWidget* addBtn = gtk_button_new_with_label("Add Plugin");
    // GTK2: use widget name + RC for "suggested" look
    gtk_widget_set_name(addBtn, "confirm-btn");
    g_signal_connect_swapped(addBtn, "clicked",
        G_CALLBACK(+[](PluginDialog* self) { self->onConfirm(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), addBtn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), btnBox, FALSE, FALSE, 0);
}

void PluginDialog::rebuildList(const std::string& filter) {
    gtk_list_store_clear(store_);

    std::string lf = filter;
    std::transform(lf.begin(), lf.end(), lf.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& entry : allPlugins_) {
        if (!lf.empty()) {
            std::string ln = entry.name;
            std::transform(ln.begin(), ln.end(), ln.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::string lu = entry.uri;
            std::transform(lu.begin(), lu.end(), lu.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (ln.find(lf) == std::string::npos &&
                lu.find(lf) == std::string::npos)
                continue;
        }

        std::string sub = entry.author.empty()
                          ? entry.uri
                          : entry.author + "  -  " + entry.uri;

        GtkTreeIter iter;
        gtk_list_store_append(store_, &iter);
        gtk_list_store_set(store_, &iter,
            COL_NAME, entry.name.c_str(),
            COL_SUB,  sub.c_str(),
            COL_URI,  entry.uri.c_str(),
            -1);
    }
}

void PluginDialog::onSearchChanged() {
    const char* text = gtk_entry_get_text(GTK_ENTRY(search_));
    rebuildList(text ? text : "");
}

void PluginDialog::onConfirm() {
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView_));
    GtkTreeModel* model = nullptr;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
        return;

    gchar* uri = nullptr;
    gtk_tree_model_get(model, &iter, COL_URI, &uri, -1);

    if (uri && confirmCb_)
        confirmCb_(std::string(uri));

    g_free(uri);

    if (dialog_ && GTK_IS_WINDOW(dialog_)) {
        gtk_widget_destroy(dialog_);
        dialog_ = nullptr;
    }
}

void PluginDialog::onCancel() {
    if (dialog_ && GTK_IS_WINDOW(dialog_)) {
        gtk_widget_destroy(dialog_);
        dialog_ = nullptr;
    }
}

void PluginDialog::show(ConfirmCb cb) {
    confirmCb_ = std::move(cb);
    gtk_widget_show_all(dialog_);
}
