// src/gtk4/PluginDialog.cpp

#include "PluginDialog.h"

#include <algorithm>
#include <cctype>
#include <cstring>

using json = nlohmann::json;

// ── PluginDialog ──────────────────────────────────────────────────────────────

PluginDialog::PluginDialog(GtkWindow* parent, const json& plugins) : parent_(parent) {
    // Build sorted plugin list from JSON
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
        gtk_window_destroy(GTK_WINDOW(dialog_));
        dialog_ = nullptr;
    }
}

void PluginDialog::buildWidgets() {
    // ── Window ────────────────────────────────────────────────────────────
    dialog_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog_), "Add Plugin");
    gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent_);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 540, 480);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);

    // ── Main vertical box ─────────────────────────────────────────────────
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(dialog_), vbox);

    // ── Search entry ──────────────────────────────────────────────────────
    search_ = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search_),
                                          "Filter plugins…");
    g_signal_connect_swapped(search_, "search-changed",
        G_CALLBACK(+[](PluginDialog* self) { self->onSearchChanged(); }),
        this);
    gtk_box_append(GTK_BOX(vbox), search_);

    // ── Scrolled plugin list ──────────────────────────────────────────────
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    listBox_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listBox_), GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(listBox_), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), listBox_);
    gtk_box_append(GTK_BOX(vbox), scroll);

    // Populate initial full list
    rebuildList("");

    // ── Button row ────────────────────────────────────────────────────────
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(btnBox), spacer);

    GtkWidget* cancelBtn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancelBtn, "clicked",
        G_CALLBACK(+[](PluginDialog* self) { self->onCancel(); }),
        this);
    gtk_box_append(GTK_BOX(btnBox), cancelBtn);

    GtkWidget* addBtn = gtk_button_new_with_label("Add Plugin");
    gtk_widget_add_css_class(addBtn, "suggested-action");
    g_signal_connect_swapped(addBtn, "clicked",
        G_CALLBACK(+[](PluginDialog* self) { self->onConfirm(); }),
        this);

    g_signal_connect(listBox_, "row-activated",
        G_CALLBACK(+[](GtkListBox* box, GtkListBoxRow* row, PluginDialog* self) {
            self->onConfirm();
        }),
        this);
    gtk_box_append(GTK_BOX(btnBox), addBtn);

    gtk_box_append(GTK_BOX(vbox), btnBox);
}

void PluginDialog::rebuildList(const std::string& filter) {
    // Remove existing rows
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(listBox_)) != nullptr)
        gtk_list_box_remove(GTK_LIST_BOX(listBox_), child);

    // Lower-case filter for case-insensitive matching
    std::string lf = filter;
    std::transform(lf.begin(), lf.end(), lf.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& entry : allPlugins_) {
        if (!lf.empty()) {
            // Match against name or URI
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

        // One row: plugin name on top, author + URI smaller below
        GtkWidget* rowBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_margin_start(rowBox, 6);
        gtk_widget_set_margin_end  (rowBox, 6);
        gtk_widget_set_margin_top  (rowBox, 4);
        gtk_widget_set_margin_bottom(rowBox, 4);

        GtkWidget* nameLbl = gtk_label_new(entry.name.c_str());
        gtk_label_set_xalign(GTK_LABEL(nameLbl), 0.0f);
        gtk_widget_add_css_class(nameLbl, "plugin-name");
        gtk_box_append(GTK_BOX(rowBox), nameLbl);

        std::string sub = entry.author.empty()
                          ? entry.uri
                          : entry.author + "  •  " + entry.uri;
        GtkWidget* subLbl = gtk_label_new(sub.c_str());
        gtk_label_set_xalign(GTK_LABEL(subLbl), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(subLbl), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(subLbl, "dim-label");
        gtk_box_append(GTK_BOX(rowBox), subLbl);

        // Store the URI on the row for retrieval on selection
        g_object_set_data_full(G_OBJECT(rowBox), "plugin-uri",
                               g_strdup(entry.uri.c_str()), g_free);

        gtk_list_box_append(GTK_LIST_BOX(listBox_), rowBox);
    }
}

void PluginDialog::onSearchChanged() {
    const char* text = gtk_editable_get_text(GTK_EDITABLE(search_));
    rebuildList(text ? text : "");
}

void PluginDialog::onConfirm() {
    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(listBox_));
    if (!row) return;

    GtkWidget* child = gtk_list_box_row_get_child(row);
    if (!child) return;

    const char* uri = static_cast<const char*>(
        g_object_get_data(G_OBJECT(child), "plugin-uri"));
    if (uri && confirmCb_)
        confirmCb_(std::string(uri));

    if (dialog_ && GTK_IS_WINDOW(dialog_)) {
        gtk_window_destroy(GTK_WINDOW(dialog_));
        dialog_ = nullptr;
    }
}

void PluginDialog::onCancel() {
    if (dialog_ && GTK_IS_WINDOW(dialog_)) {
        gtk_window_destroy(GTK_WINDOW(dialog_));
        dialog_ = nullptr;
    }
}

void PluginDialog::show(ConfirmCb cb) {
    confirmCb_ = std::move(cb);
    gtk_widget_set_visible(dialog_, TRUE);
}
