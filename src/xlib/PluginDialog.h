// src/xlib/PluginDialog.h — Xlib/Xaw port
// Modal dialog to browse/search installed LV2 plugins and confirm a choice.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <vector>
#include "XlibApp.h"

class PluginDialog {
public:
    using ConfirmCb = std::function<void(const std::string& uri)>;

    PluginDialog(Widget parent, XlibApp& app,
                 const std::string& pluginsJson);
    ~PluginDialog();

    // Show the dialog (modal).  confirmCb is called if user confirms.
    void show(ConfirmCb cb);

private:
    struct PluginEntry {
        std::string name;
        std::string uri;
    };

    void buildWidgets();
    void rebuildList(const std::string& filter);
    void onSearchKey(XEvent* ev);
    void onConfirm();
    void onCancel();

    XlibApp& app_;
    Widget   parent_;
    Widget   shell_       = nullptr;
    Widget   searchWidget_= nullptr;
    Widget   listWidget_  = nullptr;
    Widget   viewport_    = nullptr;
    Widget   confirmBtn_  = nullptr;
    Widget   cancelBtn_   = nullptr;

    std::vector<PluginEntry> allPlugins_;
    std::vector<PluginEntry> filtered_;

    std::string searchText_;
    ConfirmCb   confirmCb_;
    bool        done_ = false;

    // Xt callbacks
    static void searchKeyHandler(Widget w, XtPointer client,
                                 XEvent* ev, Boolean* dispatch);
    static void confirmCB(Widget w, XtPointer client, XtPointer call);
    static void cancelCB (Widget w, XtPointer client, XtPointer call);
    static Boolean lazyFilter(XtPointer client);  // WorkProc for deferred filter
};
