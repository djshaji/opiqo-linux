// src/xlib/SettingsDialog.h — Xlib/Xaw port
// Persistent settings dialog: audio ports, record format/quality,
// preset export/import, and plugin-cache management.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <vector>
#include "XlibApp.h"
#include "AppSettings.h"
#include "JackPortEnum.h"

class SettingsDialog {
public:
    using ApplyCb  = std::function<void(const AppSettings&)>;
    using ExportCb = std::function<void()>;
    using ImportCb = std::function<void(const std::string& path)>;
    using CacheCb  = std::function<void()>;

    SettingsDialog(Widget parent, XlibApp& app,
                   const AppSettings& current,
                   const std::vector<PortInfo>& capturePorts,
                   const std::vector<PortInfo>& playbackPorts);
    ~SettingsDialog();

    void setApplyCallback (ApplyCb  cb) { applyCb_  = std::move(cb); }
    void setExportCallback(ExportCb cb) { exportCb_ = std::move(cb); }
    void setImportCallback(ImportCb cb) { importCb_ = std::move(cb); }
    void setDeleteCacheCallback(CacheCb cb) { cacheCb_ = std::move(cb); }

    void show();
    void hide();
    bool isVisible() const { return visible_; }

    // Update port lists (after JACK reconnect)
    void updatePorts(const std::vector<PortInfo>& cap,
                     const std::vector<PortInfo>& pb);

    AppSettings getSettings() const;

private:
    void buildWidgets();
    void buildAudioTab();
    void buildPresetsTab();
    void buildPortDropdown(Widget parent, Widget* btn, Widget* menu,
                           const std::vector<PortInfo>& ports,
                           const std::string& selected,
                           std::vector<int>* itemStorage);
    void rebuildPortDropdown(Widget btn, Widget* menu,
                              const std::vector<PortInfo>& ports,
                              const std::string& selected,
                              std::vector<int>* itemStorage);
    std::string getSelectedPort(Widget btn,
                                const std::vector<PortInfo>& ports,
                                const std::vector<int>& items) const;
    void switchTab(int idx);

    // WM_DELETE_WINDOW
    static void wmDeleteHandler(Widget w, XtPointer client,
                                XEvent* ev, Boolean* dispatch);

    XlibApp& app_;
    Widget   parent_;
    Widget   shell_      = nullptr;
    Widget   audioTab_   = nullptr;
    Widget   presetsTab_ = nullptr;

    // Tab strip
    Widget tabAudioBtn_   = nullptr;
    Widget tabPresetsBtn_ = nullptr;
    int    activeTab_     = 0;

    // Audio tab widgets
    Widget cap1Btn_  = nullptr;  Widget cap1Menu_  = nullptr;
    Widget cap2Btn_  = nullptr;  Widget cap2Menu_  = nullptr;
    Widget pb1Btn_   = nullptr;  Widget pb1Menu_   = nullptr;
    Widget pb2Btn_   = nullptr;  Widget pb2Menu_   = nullptr;
    Widget srLabel_  = nullptr;
    Widget bsLabel_  = nullptr;
    Widget applyBtn_ = nullptr;
    Widget delCacheBtn_ = nullptr;

    // Preset tab widgets
    Widget exportBtn_ = nullptr;
    Widget importBtn_ = nullptr;

    std::vector<PortInfo> capturePorts_;
    std::vector<PortInfo> playbackPorts_;
    AppSettings           current_;

    // Track selected port index per dropdown (parallel to PortInfo vectors)
    std::vector<int> cap1Items_, cap2Items_, pb1Items_, pb2Items_;

    struct PortItem { SettingsDialog* dlg; int index; Widget btn;
                      std::vector<PortInfo>* ports;
                      std::vector<int>* items; };
    std::vector<PortItem*> portItems_;

    bool visible_ = false;

    ApplyCb  applyCb_;
    ExportCb exportCb_;
    ImportCb importCb_;
    CacheCb  cacheCb_;
};
