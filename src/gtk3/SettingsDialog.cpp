// src/gtk3/SettingsDialog.cpp (GTK3 port)

#include "SettingsDialog.h"
#include "logging_macros.h"

#include <cstdio>
#include <fstream>

static void deletePluginCache() {
    const std::string path = std::string(g_dirname((char*)AppSettings::configPath().c_str()))
                             + "/opiqo_plugin_cache.json";
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
        // GTK3: gtk_widget_destroy instead of gtk_window_destroy
        gtk_widget_destroy(dialog_);
        dialog_ = nullptr;
    }
}

void SettingsDialog::show() {
    if (dialog_) {
        gtk_widget_show_all(dialog_);
    }
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
    dialog_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog_), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog_), parent_);
    gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 480, 360);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog_), TRUE);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // GTK3: gtk_container_add instead of gtk_window_set_child
    gtk_container_add(GTK_CONTAINER(dialog_), vbox);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

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
    gtk_box_pack_start(GTK_BOX(btnBox), spacer, TRUE, TRUE, 0);

    GtkWidget* closeBtn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(closeBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onClose(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), closeBtn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), btnBox, FALSE, FALSE, 0);
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
    gtk_box_pack_start(GTK_BOX(page), grid, FALSE, FALSE, 0);

    // GTK3: GtkComboBoxText instead of GtkDropDown
    capDrop_ = gtk_combo_box_text_new();
    populatePortDropDown(capDrop_, capPorts_, settings_.capturePort);
    makeRow(grid, 0, "Capture L", capDrop_);

    capDrop2_ = gtk_combo_box_text_new();
    populatePortDropDown(capDrop2_, capPorts_, settings_.capturePort2);
    makeRow(grid, 1, "Capture R", capDrop2_);

    pbkDrop_ = gtk_combo_box_text_new();
    populatePortDropDown(pbkDrop_, pbkPorts_, settings_.playbackPort);
    makeRow(grid, 2, "Playback L", pbkDrop_);

    pbkDrop2_ = gtk_combo_box_text_new();
    populatePortDropDown(pbkDrop2_, pbkPorts_, settings_.playbackPort2);
    makeRow(grid, 3, "Playback R", pbkDrop2_);

    srLabel_ = gtk_label_new("—");
    gtk_label_set_xalign(GTK_LABEL(srLabel_), 0.0f);
    makeRow(grid, 4, "Sample Rate", srLabel_);

    bsLabel_ = gtk_label_new("—");
    gtk_label_set_xalign(GTK_LABEL(bsLabel_), 0.0f);
    makeRow(grid, 5, "Block Size", bsLabel_);

    GtkWidget* applyBtn = gtk_button_new_with_label("Apply");
    // GTK3: gtk_style_context_add_class
    gtk_style_context_add_class(
        gtk_widget_get_style_context(applyBtn), "suggested-action");
    gtk_widget_set_margin_top(applyBtn, 8);
    g_signal_connect_swapped(applyBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onApply(); }), this);
    gtk_box_pack_start(GTK_BOX(page), applyBtn, FALSE, FALSE, 0);

    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 12);
    gtk_box_pack_start(GTK_BOX(page), sep, FALSE, FALSE, 0);

    GtkWidget* deletePluginCacheBtn = gtk_button_new_with_label("Delete Plugin Cache");
    gtk_widget_set_tooltip_text(deletePluginCacheBtn,
        "Delete the cached plugin info file (will be recreated on next launch)");
    g_signal_connect_swapped(deletePluginCacheBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* /*self*/) { deletePluginCache(); }), this);
    gtk_box_pack_start(GTK_BOX(page), deletePluginCacheBtn, FALSE, FALSE, 0);

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
    // GTK3: gtk_label_set_line_wrap instead of gtk_label_set_wrap
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
    gtk_box_pack_start(GTK_BOX(page), desc, FALSE, FALSE, 0);

    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget* exportBtn = gtk_button_new_with_label("Export Preset…");
    g_signal_connect_swapped(exportBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onExport(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), exportBtn, FALSE, FALSE, 0);

    GtkWidget* importBtn = gtk_button_new_with_label("Import Preset…");
    g_signal_connect_swapped(importBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onImport(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), importBtn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(page), btnBox, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page,
                             gtk_label_new("Presets"));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

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

// ── Actions ───────────────────────────────────────────────────────────────────

void SettingsDialog::onApply() {
    settings_.capturePort   = selectedPort(capDrop_,  capPorts_);
    settings_.capturePort2  = selectedPort(capDrop2_, capPorts_);
    settings_.playbackPort  = selectedPort(pbkDrop_,  pbkPorts_);
    settings_.playbackPort2 = selectedPort(pbkDrop2_, pbkPorts_);

    if (applyCb_) applyCb_(settings_);
}

void SettingsDialog::onClose() {
    gtk_widget_hide(dialog_);
}

void SettingsDialog::onExport() {
    if (!exportCb_) return;
    const std::string jsonData = exportCb_();
    if (jsonData.empty()) return;

    // GTK3: synchronous GtkFileChooserDialog instead of async GtkFileDialog
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Export Preset", GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save",   GTK_RESPONSE_ACCEPT,
        nullptr);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), "opiqo-preset.json");

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (path) {
            std::ofstream f(path);
            if (f.is_open()) f << jsonData;
            g_free(path);
        }
    }
    gtk_widget_destroy(dlg);
}

void SettingsDialog::onImport() {
    if (!importCb_) return;

    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Import Preset", GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT,
        nullptr);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (path) {
            importCb_(std::string(path));
            g_free(path);
        }
    }
    gtk_widget_destroy(dlg);
}
