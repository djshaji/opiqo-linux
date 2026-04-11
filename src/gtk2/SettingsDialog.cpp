// src/gtk2/SettingsDialog.cpp (GTK2 port)

#include "SettingsDialog.h"
#include "logging_macros.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <libgen.h>

static void deletePluginCache() {
    gchar* dir = g_path_get_dirname(AppSettings::configPath().c_str());
    std::string path = std::string(dir) + "/opiqo_plugin_cache.json";
    g_free(dir);
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
        gtk_widget_destroy(dialog_);
        dialog_ = nullptr;
    }
}

void SettingsDialog::show() {
    if (dialog_)
        gtk_widget_show_all(dialog_);
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

    // GTK2: gtk_vbox_new instead of gtk_box_new(VERTICAL, ...)
    GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(dialog_), vbox);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    buildAudioTab(notebook);
    buildPresetsTab(notebook);

    // ── Close button ──────────────────────────────────────────────────────
    GtkWidget* btnBox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(btnBox), 6);

    // Spacer
    gtk_box_pack_start(GTK_BOX(btnBox), gtk_hbox_new(FALSE, 0), TRUE, TRUE, 0);

    GtkWidget* closeBtn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(closeBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onClose(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), closeBtn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), btnBox, FALSE, FALSE, 0);
}

// Helper to attach a label + control pair to a GtkTable row
static void tableRow(GtkWidget* table, int row,
                     const char* labelText, GtkWidget* control) {
    GtkWidget* lbl = gtk_label_new(labelText);
    // GTK2: gtk_misc_set_alignment instead of gtk_label_set_xalign
    gtk_misc_set_alignment(GTK_MISC(lbl), 1.0f, 0.5f);
    gtk_table_attach(GTK_TABLE(table), lbl,
        0, 1, row, row + 1,
        GTK_FILL, GTK_FILL, 4, 2);
    gtk_table_attach(GTK_TABLE(table), control,
        1, 2, row, row + 1,
        (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 4, 2);
}

void SettingsDialog::buildAudioTab(GtkWidget* notebook) {
    // GTK2: gtk_vbox_new
    GtkWidget* page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    // GTK2: gtk_table_new instead of gtk_grid_new
    GtkWidget* table = gtk_table_new(6, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(page), table, FALSE, FALSE, 0);

    capDrop_ = gtk_combo_box_text_new();
    populatePortDropDown(capDrop_, capPorts_, settings_.capturePort);
    tableRow(table, 0, "Capture L", capDrop_);

    capDrop2_ = gtk_combo_box_text_new();
    populatePortDropDown(capDrop2_, capPorts_, settings_.capturePort2);
    tableRow(table, 1, "Capture R", capDrop2_);

    pbkDrop_ = gtk_combo_box_text_new();
    populatePortDropDown(pbkDrop_, pbkPorts_, settings_.playbackPort);
    tableRow(table, 2, "Playback L", pbkDrop_);

    pbkDrop2_ = gtk_combo_box_text_new();
    populatePortDropDown(pbkDrop2_, pbkPorts_, settings_.playbackPort2);
    tableRow(table, 3, "Playback R", pbkDrop2_);

    srLabel_ = gtk_label_new("—");
    gtk_misc_set_alignment(GTK_MISC(srLabel_), 0.0f, 0.5f);
    tableRow(table, 4, "Sample Rate", srLabel_);

    bsLabel_ = gtk_label_new("—");
    gtk_misc_set_alignment(GTK_MISC(bsLabel_), 0.0f, 0.5f);
    tableRow(table, 5, "Block Size", bsLabel_);

    GtkWidget* applyBtn = gtk_button_new_with_label("Apply");
    g_signal_connect_swapped(applyBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onApply(); }), this);
    gtk_box_pack_start(GTK_BOX(page), applyBtn, FALSE, FALSE, 4);

    // GTK2: gtk_hseparator_new instead of gtk_separator_new(HORIZONTAL)
    gtk_box_pack_start(GTK_BOX(page), gtk_hseparator_new(), FALSE, FALSE, 4);

    GtkWidget* deletePluginCacheBtn = gtk_button_new_with_label("Delete Plugin Cache");
    gtk_widget_set_tooltip_text(deletePluginCacheBtn,
        "Delete the cached plugin info file (will be recreated on next launch)");
    g_signal_connect_swapped(deletePluginCacheBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* /*self*/) { deletePluginCache(); }), this);
    gtk_box_pack_start(GTK_BOX(page), deletePluginCacheBtn, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new("Audio"));
}

void SettingsDialog::buildPresetsTab(GtkWidget* notebook) {
    GtkWidget* page = gtk_vbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    GtkWidget* desc = gtk_label_new(
        "Export the current four plugin slots to a JSON preset file, "
        "or import a previously saved preset.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    // GTK2: gtk_misc_set_alignment for left-align
    gtk_misc_set_alignment(GTK_MISC(desc), 0.0f, 0.5f);
    gtk_box_pack_start(GTK_BOX(page), desc, FALSE, FALSE, 0);

    GtkWidget* btnBox = gtk_hbox_new(FALSE, 8);

    GtkWidget* exportBtn = gtk_button_new_with_label("Export Preset...");
    g_signal_connect_swapped(exportBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onExport(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), exportBtn, FALSE, FALSE, 0);

    GtkWidget* importBtn = gtk_button_new_with_label("Import Preset...");
    g_signal_connect_swapped(importBtn, "clicked",
        G_CALLBACK(+[](SettingsDialog* self) { self->onImport(); }), this);
    gtk_box_pack_start(GTK_BOX(btnBox), importBtn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(page), btnBox, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new("Presets"));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void SettingsDialog::populatePortDropDown(GtkWidget* dd,
                                          const std::vector<PortInfo>& ports,
                                          const std::string& selected) {
    for (const auto& p : ports)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dd), p.friendlyName.c_str());

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

    // GTK2: use GTK_STOCK_* constants for file chooser buttons
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        "Export Preset", GTK_WINDOW(dialog_),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_SAVE,   GTK_RESPONSE_ACCEPT,
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
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
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
