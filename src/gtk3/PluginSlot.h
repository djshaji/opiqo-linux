// src/gtk3/PluginSlot.h (GTK3 port)
// One of the four plugin slots in the 2×2 grid.

#pragma once

#include <functional>
#include <gtk/gtk.h>
#include <string>

#include "ParameterPanel.h"
#include "LV2Plugin.hpp"

class PluginSlot {
public:
    using AddCb    = std::function<void(int slot)>;
    using DeleteCb = std::function<void(int slot)>;
    using BypassCb = std::function<void(int slot, bool bypassed)>;
    using ValueCb  = std::function<void(int slot, uint32_t portIndex, float value)>;
    using FileCb   = std::function<void(int slot, const std::string& uri,
                                         const std::string& path)>;

    PluginSlot(int slot, GtkWidget* parent_window);
    ~PluginSlot() = default;

    GtkWidget* widget() const { return frame_; }

    void setAddCallback   (AddCb    cb) { addCb_    = std::move(cb); }
    void setDeleteCallback(DeleteCb cb) { deleteCb_ = std::move(cb); }
    void setBypassCallback(BypassCb cb) { bypassCb_ = std::move(cb); }
    void setValueCallback (ValueCb  cb) { valueCb_  = std::move(cb); }
    void setFileCallback  (FileCb   cb) { fileCb_   = std::move(cb); }

    void onPluginAdded(const std::string& pluginName,
                       const std::vector<LV2Plugin::PortInfo>& ports);
    void onPluginCleared();

    int slotIndex() const { return slot_; }

private:
    void buildWidgets();

    int slot_;

    GtkWidget* frame_        = nullptr;
    GtkWidget* headerBox_    = nullptr;
    GtkWidget* nameLabel_    = nullptr;
    GtkWidget* addButton_    = nullptr;
    GtkWidget* bypassButton_ = nullptr;
    GtkWidget* deleteButton_ = nullptr;

    ParameterPanel* paramPanel_ = nullptr;

    AddCb    addCb_;
    DeleteCb deleteCb_;
    BypassCb bypassCb_;
    ValueCb  valueCb_;
    FileCb   fileCb_;
};
