// src/xlib/PresetBar.h — Xlib/Xaw port
// Horizontal bar for named preset management.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <vector>
#include "XlibApp.h"

class PresetBar {
public:
    using LoadCb   = std::function<void()>;
    using SaveCb   = std::function<void(const std::string& name)>;
    using DeleteCb = std::function<void()>;

    PresetBar(Widget parent, XlibApp& app);
    ~PresetBar();

    Widget widget() const { return bar_; }

    void setLoadCallback  (LoadCb   cb) { loadCb_   = std::move(cb); }
    void setSaveCallback  (SaveCb   cb) { saveCb_   = std::move(cb); }
    void setDeleteCallback(DeleteCb cb) { deleteCb_ = std::move(cb); }

    void setPresetNames(const std::vector<std::string>& names);
    void setCurrentName(const std::string& name);
    std::string getCurrentName() const;
    int  getSelectedIndex() const { return selectedIndex_; }

    // Called when a preset entry is selected from the popup menu
    void onPresetSelected(int idx);

private:
    void buildWidgets(Widget parent);
    void buildPresetMenu();
    void destroyPresetMenu();

    XlibApp& app_;
    Widget   bar_        = nullptr;
    Widget   presetBtn_  = nullptr;     // MenuButton showing current preset
    Widget   presetMenu_ = nullptr;     // SimpleMenu popup
    Widget   nameEntry_  = nullptr;     // AsciiText for preset name
    Widget   loadBtn_    = nullptr;
    Widget   saveBtn_    = nullptr;
    Widget   deleteBtn_  = nullptr;

    int      selectedIndex_ = -1;
    std::vector<std::string> presetNames_;

    struct PresetItem { PresetBar* bar; int index; };
    std::vector<PresetItem*> presetItems_;

    LoadCb   loadCb_;
    SaveCb   saveCb_;
    DeleteCb deleteCb_;
};
