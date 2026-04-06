// src/linux/ParameterPanel.h
// Dynamically builds GTK4 controls from LV2Plugin::PortInfo.
// Contained inside a GtkScrolledWindow; rebuilt whenever a plugin is loaded/removed.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <gtk/gtk.h>

#include "logging_macros.h"
#include "LV2Plugin.hpp"

class ParameterPanel {
public:
    // valueCb(portIndex, value)  — fires on the GTK main thread
    // fileCb(writableUri, path)  — fires on the GTK main thread (AtomFilePath)
    using ValueCb = std::function<void(uint32_t portIndex, float value)>;
    using FileCb  = std::function<void(const std::string& writableUri,
                                       const std::string& path)>;

    // parent_window is needed to parent file-chooser dialogs.
    explicit ParameterPanel(GtkWidget* parent_window);
    ~ParameterPanel() = default;

    GtkWidget* widget() const { return scroll_; }

    void setValueCallback(ValueCb cb) { valueCb_ = std::move(cb); }
    void setFileCallback (FileCb  cb) { fileCb_  = std::move(cb); }

    // (Re)build the panel from a new PortInfo list.
    void build(const std::vector<LV2Plugin::PortInfo>& ports);

    // Remove all controls (called on plugin delete).
    void clear();

private:
    // Per-control callback data (heap-allocated, freed when panel is cleared)
    struct ControlData {
        ParameterPanel* panel;
        uint32_t        portIndex;
        std::string     writableUri; // non-empty for AtomFilePath
    };

    static void onScaleChanged(GtkRange* range, gpointer data);
    static void onToggleChanged(GtkCheckButton* btn, gpointer data);
    static void onTriggerClicked(GtkButton* btn, gpointer data);
    static void onBrowseClicked(GtkButton* btn, gpointer data);

    GtkWidget* scroll_  = nullptr;
    GtkWidget* box_     = nullptr;
    GtkWidget* parent_  = nullptr;   // for file dialog parenting

    ValueCb valueCb_;
    FileCb  fileCb_;

    std::vector<ControlData*> controlDataList_; // owned, freed in clear()
};
