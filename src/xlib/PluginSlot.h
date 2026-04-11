// src/xlib/PluginSlot.h — Xlib/Xaw port
// One slot in the 2×2 plugin grid.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>
#include "XlibApp.h"
#include "logging_macros.h"
#include "LV2Plugin.hpp"

class ParameterPanel;

class PluginSlot {
public:
    using AddCb    = std::function<void(int slot)>;
    using DeleteCb = std::function<void(int slot)>;
    using BypassCb = std::function<void(int slot, bool bypassed)>;
    using SetValCb = std::function<void(int slot, uint32_t portIdx, float value)>;
    using SetFileCb= std::function<void(int slot, uint32_t portIdx, const std::string& path)>;

    PluginSlot(int slot, Widget parent, XlibApp& app);
    ~PluginSlot();

    Widget widget() const { return frame_; }

    void setAddCallback    (AddCb     cb) { addCb_    = std::move(cb); }
    void setDeleteCallback (DeleteCb  cb) { deleteCb_ = std::move(cb); }
    void setBypassCallback (BypassCb  cb) { bypassCb_ = std::move(cb); }
    void setSetValueCallback(SetValCb cb) { setValCb_ = std::move(cb); }
    void setSetFileCallback (SetFileCb cb){ setFileCb_= std::move(cb); }

    // Called from MainWindow when engine reports plugin added/cleared
    void onPluginAdded (const std::string& name,
                        const std::string& portsJson);
    void onPluginAdded (const std::string& name,
                        const std::vector<LV2Plugin::PortInfo>& ports);
    void onPluginCleared();

    // State setters
    void setBypassed(bool b);
    bool isBypassed() const { return bypassed_; }

    int slot() const { return slot_; }

private:
    void buildWidgets(Widget parent);

    int      slot_;
    XlibApp& app_;

    Widget   frame_        = nullptr;  // Form with border
    Widget   headerBox_    = nullptr;  // horizontal Box: label + buttons
    Widget   nameLabel_    = nullptr;
    Widget   bypassToggle_ = nullptr;
    Widget   addBtn_       = nullptr;
    Widget   deleteBtn_    = nullptr;
    Widget   paramArea_    = nullptr;  // Viewport for ParameterPanel

    ParameterPanel* params_ = nullptr;

    bool bypassed_    = false;
    bool pluginLoaded_= false;

    AddCb    addCb_;
    DeleteCb deleteCb_;
    BypassCb bypassCb_;
    SetValCb setValCb_;
    SetFileCb setFileCb_;
};
