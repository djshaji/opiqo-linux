// src/gtk4/SettingsDialog.cpp

#include "SettingsDialog.h"
#include "logging_macros.h"

#include <cstdio>
#include <fstream>

void deletePluginCache () {
    const std::string path = std::string (g_dirname ((char *)AppSettings::configPath().c_str())) + "/opiqo_plugin_cache.json";
    if (remove(path.c_str()) == 0) {
        LOGD("Plugin cache file deleted: %s", path.c_str());
    } else {
        LOGD("No plugin cache file to delete at: %s", path.c_str());
    }
}

// ── SettingsDialog ────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(GtkWindow* parent,
                               const AppSettings& current,
                               const std::vector<PortInfo>& capturePorts,
                               const std::vector<PortInfo>& playbackPorts)
    : parent_(parent), settings_(current),
      capPorts_(capturePorts), pbkPorts_(playbackPorts) {
    buildWidgets();
}

SettingsDialog::~SettingsDialog() {
    if (dialog_) {
        gtk_window_destroy(GTK_WINDOW(dialog_));
        dialog_ = nullptr;
    }
}

void SettingsDialog::show() {
    if (dialog_) gtk_widget_set_visible(dialog_, TRUE);
}

void SettingsDialog::updateAudioInfo(int sampleRate, int blockSize) {
    if (!srLabel_ || !bsLabel_) return;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d Hz", sampleRate);
    gtk_label_set_text(GTK_LABEL(srLabel_), buf);
    std::snprintf(buf, sizeof(buf), "%d frames", blockSize);
    gtk_label_set_text(GTK_LABEL(bsLabel_), buf);
}

// ── Build ─────────────────────────────────────────────────────────────────────

void SettingsDialog::buildWidgets() {
    dialog_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog_), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent_);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 480, 360);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(dialog_), vbox);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_append(GTK_BOX(vbox), notebook);

    buildAudioTab(notebook);
    buildPresetsTab(notebook);

    // ── Close button ─────────────────────────────────────────────────────
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(btnBox, 8);
    gtk_widget_set_margin_end(btnBox, 8);
    gtk_widget_set_margin_top(btnBox, 4);
    gtk_widget_set_margin_bottom(btnBox, 6);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(btnBox), spacer);

    GtkWidget* closeBtn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(closeBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onClose(); }), this);
    gtk_box_append(GTK_BOX(btnBox), closeBtn);

    gtk_box_append(GTK_BOX(vbox), btnBox);
}

static GtkWidget* makeRow(GtkWidget* grid, int row,
                           const char* labelText, GtkWidget* control) {
    GtkWidget* lbl = gtk_label_new(labelText);
    gtk_label_set_xalign(GTK_LABEL(lbl), 1.0f);
    gtk_widget_set_margin_end(lbl, 8);
    gtk_grid_attach(GTK_GRID(grid), lbl,    0, row, 1, 1);
    gtk_widget_set_hexpand(control, TRUE);
    gtk_grid_attach(GTK_GRID(grid), control, 1, row, 1, 1);
    return control;
}

void SettingsDialog::buildAudioTab(GtkWidget* notebook) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start (page, 12);
    gtk_widget_set_margin_end   (page, 12);
    gtk_widget_set_margin_top   (page, 12);
    gtk_widget_set_margin_bottom(page, 12);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_append(GTK_BOX(page), grid);

    // ── Capture ports ─────────────────────────────────────────────────────
    capDrop_ = gtk_drop_down_new(nullptr, nullptr);
    populatePortDropDown(capDrop_, capPorts_, settings_.capturePort);
    makeRow(grid, 0, "Capture L", capDrop_);

    capDrop2_ = gtk_drop_down_new(nullptr, nullptr);
    populatePortDropDown(capDrop2_, capPorts_, settings_.capturePort2);
    makeRow(grid, 1, "Capture R", capDrop2_);

    // ── Playback ports ────────────────────────────────────────────────────
    pbkDrop_ = gtk_drop_down_new(nullptr, nullptr);
    populatePortDropDown(pbkDrop_, pbkPorts_, settings_.playbackPort);
    makeRow(grid, 2, "Playback L", pbkDrop_);

    pbkDrop2_ = gtk_drop_down_new(nullptr, nullptr);
    populatePortDropDown(pbkDrop2_, pbkPorts_, settings_.playbackPort2);
    makeRow(grid, 3, "Playback R", pbkDrop2_);

    // ── Read-only server info ─────────────────────────────────────────────
    srLabel_ = gtk_label_new("—");
    gtk_label_set_xalign(GTK_LABEL(srLabel_), 0.0f);
    makeRow(grid, 4, "Sample Rate", srLabel_);

    bsLabel_ = gtk_label_new("—");
    gtk_label_set_xalign(GTK_LABEL(bsLabel_), 0.0f);
    makeRow(grid, 5, "Block Size", bsLabel_);

    // ── Apply button ──────────────────────────────────────────────────────
    GtkWidget* applyBtn = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(applyBtn, "suggested-action");
    gtk_widget_set_margin_top(applyBtn, 8);
    g_signal_connect_swapped(applyBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onApply(); }), this);
    gtk_box_append(GTK_BOX(page), applyBtn);

    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 12);
    gtk_box_append(GTK_BOX(page), sep);

    GtkWidget * deletePluginCacheBtn = gtk_button_new_with_label("Delete Plugin Cache");
    gtk_widget_set_tooltip_text(deletePluginCacheBtn, "Delete the cached plugin info file (for debugging; the file will be recreated on next app launch)");
    g_signal_connect_swapped(deletePluginCacheBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { deletePluginCache(); }), this);
    gtk_box_append(GTK_BOX(page), deletePluginCacheBtn);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page,
                             gtk_label_new("Audio"));
}

void SettingsDialog::buildPresetsTab(GtkWidget* notebook) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start (page, 12);
    gtk_widget_set_margin_end   (page, 12);
    gtk_widget_set_margin_top   (page, 12);
    gtk_widget_set_margin_bottom(page, 12);

    GtkWidget* desc = gtk_label_new(
        "Export the current four plugin slots to a JSON preset file, "
        "or import a previously saved preset.");
    gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
    gtk_box_append(GTK_BOX(page), desc);

    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget* exportBtn = gtk_button_new_with_label("Export Preset…");
    g_signal_connect_swapped(exportBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onExport(); }), this);
    gtk_box_append(GTK_BOX(btnBox), exportBtn);

    GtkWidget* importBtn = gtk_button_new_with_label("Import Preset…");
    g_signal_connect_swapped(importBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onImport(); }), this);
    gtk_box_append(GTK_BOX(btnBox), importBtn);

    gtk_box_append(GTK_BOX(page), btnBox);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page,
                             gtk_label_new("Presets"));
}

// ── Helper: populate a GtkDropDown from PortInfo list ─────────────────────────

void SettingsDialog::populatePortDropDown(GtkWidget* dd,
                                          const std::vector<PortInfo>& ports,
                                          const std::string& selected) {
    std::vector<const char*> names;
    for (const auto& p : ports)
        names.push_back(p.friendlyName.c_str());
    names.push_back(nullptr);

    GtkStringList* sl = gtk_string_list_new(names.data());
    gtk_drop_down_set_model(GTK_DROP_DOWN(dd), G_LIST_MODEL(sl));

    // Set selection
    guint sel = 0;
    for (guint i = 0; i < ports.size(); ++i) {
        if (ports[i].id == selected) { sel = i; break; }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), sel);
}

std::string SettingsDialog::selectedPort(GtkWidget* dd,
                                          const std::vector<PortInfo>& ports) const {
    const guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    return (idx < ports.size()) ? ports[idx].id : "";
}

// ── Actions ───────────────────────────────────────────────────────────────────

void SettingsDialog::onApply() {
    settings_.capturePort   = selectedPort(capDrop_,  capPorts_);
    settings_.capturePort2  = selectedPort(capDrop2_, capPorts_);
    settings_.playbackPort  = selectedPort(pbkDrop_,  pbkPorts_);
    settings_.playbackPort2 = selectedPort(pbkDrop2_, pbkPorts_);

    if (applyCb_) applyCb_(settings_);
}

void SettingsDialog::onClose() {
    gtk_widget_set_visible(dialog_, FALSE);
}

void SettingsDialog::onExport() {
    if (!exportCb_) return;
    const std::string jsonData = exportCb_();
    if (jsonData.empty()) return;

    GtkFileDialog* fd = gtk_file_dialog_new();
    gtk_file_dialog_set_title(fd, "Export Preset");
    gtk_file_dialog_set_initial_name(fd, "opiqo-preset.json");

    struct SaveCtx { std::string data; };
    auto* ctx = new SaveCtx{jsonData};

    gtk_file_dialog_save(fd,
        GTK_WINDOW(dialog_),
        nullptr,
        [](GObject* src, GAsyncResult* res, gpointer user_data) {
            auto* ctx = static_cast<SaveCtx*>(user_data);
            GError* err = nullptr;
            GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, &err);
            if (file) {
                char* path = g_file_get_path(file);
                if (path) {
                    std::ofstream f(path);
                    if (f.is_open()) f << ctx->data;
                    g_free(path);
                }
                g_object_unref(file);
            }
            if (err) g_error_free(err);
            delete ctx;
        },
        ctx);
    g_object_unref(fd);
}

void SettingsDialog::onImport() {
    if (!importCb_) return;

    GtkFileDialog* fd = gtk_file_dialog_new();
    gtk_file_dialog_set_title(fd, "Import Preset");

    struct ImportCtx { SettingsDialog* self; };
    auto* ctx = new ImportCtx{this};

    gtk_file_dialog_open(fd,
        GTK_WINDOW(dialog_),
        nullptr,
        [](GObject* src, GAsyncResult* res, gpointer user_data) {
            auto* ctx = static_cast<ImportCtx*>(user_data);
            GError* err = nullptr;
            GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &err);
            if (file) {
                char* path = g_file_get_path(file);
                if (path && ctx->self->importCb_)
                    ctx->self->importCb_(std::string(path));
                g_free(path);
                g_object_unref(file);
            }
            if (err) g_error_free(err);
            delete ctx;
        },
        ctx);
    g_object_unref(fd);
}
