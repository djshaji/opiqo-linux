// src/gtk2/ParameterPanel.h (GTK2 port)
// Dynamically builds GTK2 controls from LV2Plugin::PortInfo.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <gtk/gtk.h>

#include "logging_macros.h"
#include "LV2Plugin.hpp"

class ParameterPanel {
public:
    using ValueCb = std::function<void(uint32_t portIndex, float value)>;
    using FileCb  = std::function<void(const std::string& writableUri,
                                       const std::string& path)>;

    explicit ParameterPanel(GtkWidget* parent_window);
    ~ParameterPanel() = default;

    GtkWidget* widget() const { return scroll_; }

    void setValueCallback(ValueCb cb) { valueCb_ = std::move(cb); }
    void setFileCallback (FileCb  cb) { fileCb_  = std::move(cb); }

    void build(const std::vector<LV2Plugin::PortInfo>& ports);
    void clear();

private:
    struct ControlData {
        ParameterPanel* panel;
        uint32_t        portIndex;
        std::string     writableUri;
    };

    static void onScaleChanged  (GtkRange*        range, gpointer data);
    static void onToggleChanged (GtkToggleButton* btn,   gpointer data);
    static void onTriggerClicked(GtkButton*       btn,   gpointer data);
    static void onBrowseClicked (GtkButton*       btn,   gpointer data);

    GtkWidget* scroll_  = nullptr;
    GtkWidget* box_     = nullptr;
    GtkWidget* parent_  = nullptr;

    ValueCb valueCb_;
    FileCb  fileCb_;

    std::vector<ControlData*> controlDataList_;
};
