// src/gtk3/SettingsDialog.h (GTK3 port)
// Two-tab dialog: Audio (JACK port selection) and Presets (export / import).

#pragma once

#include <functional>
#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "AppSettings.h"
#include "JackPortEnum.h"

class SettingsDialog {
public:
    using ApplyCb  = std::function<void(const AppSettings& newSettings)>;
    using ImportCb = std::function<void(const std::string& jsonPath)>;
    using ExportCb = std::function<std::string()>;

    SettingsDialog(GtkWindow* parent,
                   const AppSettings& current,
                   const std::vector<PortInfo>& capturePorts,
                   const std::vector<PortInfo>& playbackPorts);
    ~SettingsDialog();

    void setApplyCallback (ApplyCb  cb) { applyCb_  = std::move(cb); }
    void setImportCallback(ImportCb cb) { importCb_ = std::move(cb); }
    void setExportCallback(ExportCb cb) { exportCb_ = std::move(cb); }

    void show();
    void updateAudioInfo(int sampleRate, int blockSize);

private:
    void buildWidgets();
    void buildAudioTab(GtkWidget* notebook);
    void buildPresetsTab(GtkWidget* notebook);

    void populatePortDropDown(GtkWidget* dd,
                              const std::vector<PortInfo>& ports,
                              const std::string& selected);
    std::string selectedPort(GtkWidget* dd,
                             const std::vector<PortInfo>& ports) const;

    void onApply();
    void onClose();
    void onExport();
    void onImport();

    GtkWindow*  parent_  = nullptr;
    GtkWidget*  dialog_  = nullptr;

    GtkWidget* capDrop_   = nullptr;
    GtkWidget* capDrop2_  = nullptr;
    GtkWidget* pbkDrop_   = nullptr;
    GtkWidget* pbkDrop2_  = nullptr;
    GtkWidget* srLabel_   = nullptr;
    GtkWidget* bsLabel_   = nullptr;

    AppSettings              settings_;
    std::vector<PortInfo>    capPorts_;
    std::vector<PortInfo>    pbkPorts_;

    ApplyCb  applyCb_;
    ImportCb importCb_;
    ExportCb exportCb_;
};
