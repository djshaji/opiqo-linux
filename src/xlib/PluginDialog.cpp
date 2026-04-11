// src/xlib/PluginDialog.cpp — Xlib/Xaw port
// Modal dialog to browse and select an LV2 plugin.

#include "PluginDialog.h"

#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/List.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string toLower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)tolower((unsigned char)c);
    return r;
}

// ── Constructor / destructor ──────────────────────────────────────────────────

PluginDialog::PluginDialog(Widget parent, XlibApp& app,
                           const std::string& pluginsJson)
    : app_(app), parent_(parent) {
    try {
        json plugins = json::parse(pluginsJson);
        for (auto it = plugins.begin(); it != plugins.end(); ++it) {
            PluginEntry e;
            e.uri  = it.key();
            e.name = it.value().value("name", e.uri);
            allPlugins_.push_back(std::move(e));
        }
    } catch (...) {}

    std::sort(allPlugins_.begin(), allPlugins_.end(),
              [](const PluginEntry& a, const PluginEntry& b) {
                  return a.name < b.name;
              });

    buildWidgets();
}

PluginDialog::~PluginDialog() {
    if (shell_) XtDestroyWidget(shell_);
}

// ── Widget construction ───────────────────────────────────────────────────────

void PluginDialog::buildWidgets() {
    shell_ = XtVaCreatePopupShell("addPlugin",
        transientShellWidgetClass, parent_,
        XtNtitle, "Add Plugin",
        XtNwidth, 540,
        XtNheight, 480,
        nullptr);

    Widget form = XtVaCreateManagedWidget("form",
        formWidgetClass, shell_,
        XtNdefaultDistance, 6,
        nullptr);

    // ── Search row ────────────────────────────────────────────────────────
    Widget searchLbl = XtVaCreateManagedWidget("Filter:",
        labelWidgetClass, form,
        XtNborderWidth, 0,
        XtNtop,  XawChainTop,
        XtNleft, XawChainLeft,
        nullptr);

    searchWidget_ = XtVaCreateManagedWidget("search",
        asciiTextWidgetClass, form,
        XtNeditType, XawtextEdit,
        XtNwidth, 400,
        XtNfromHoriz, searchLbl,
        XtNtop,  XawChainTop,
        XtNleft, XawChainLeft,
        nullptr);

    // Capture key releases to filter the list
    XtAddEventHandler(searchWidget_, KeyReleaseMask, False,
        searchKeyHandler, this);

    // ── List in viewport ──────────────────────────────────────────────────
    viewport_ = XtVaCreateManagedWidget("viewport",
        viewportWidgetClass, form,
        XtNfromVert, searchLbl,
        XtNwidth,    520,
        XtNheight,   360,
        XtNallowVert, True,
        XtNtop,    XawChainTop,
        XtNleft,   XawChainLeft,
        XtNright,  XawChainRight,
        XtNbottom, XawRubber,
        nullptr);

    listWidget_ = XtVaCreateManagedWidget("list",
        listWidgetClass, viewport_,
        XtNdefaultColumns, 1,
        XtNforceColumns,   True,
        nullptr);

    XtAddCallback(listWidget_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            // Double-click or single-click: just track selection.
            // Actual confirm on button press.
            (void)client;
        }, this);

    // ── Button row ────────────────────────────────────────────────────────
    Widget btnBox = XtVaCreateManagedWidget("buttons",
        boxWidgetClass, form,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        XtNfromVert, viewport_,
        XtNtop,    XawChainBottom,
        XtNleft,   XawChainLeft,
        XtNbottom, XawChainBottom,
        nullptr);

    cancelBtn_ = XtVaCreateManagedWidget("Cancel",
        commandWidgetClass, btnBox, nullptr);
    XtAddCallback(cancelBtn_, XtNcallback, cancelCB, this);

    confirmBtn_ = XtVaCreateManagedWidget("Add",
        commandWidgetClass, btnBox,
        XtNforeground, app_.colors().blue,
        nullptr);
    XtAddCallback(confirmBtn_, XtNcallback, confirmCB, this);

    rebuildList("");
}

// ── List management ───────────────────────────────────────────────────────────

void PluginDialog::rebuildList(const std::string& filter) {
    filtered_.clear();
    std::string f = toLower(filter);

    for (const auto& e : allPlugins_) {
        if (f.empty() || toLower(e.name).find(f) != std::string::npos
                      || toLower(e.uri).find(f) != std::string::npos) {
            filtered_.push_back(e);
        }
    }

    // Xaw List expects a null-terminated array of char*
    static std::vector<const char*> ptrs;
    ptrs.clear();
    for (const auto& e : filtered_)
        ptrs.push_back(e.name.c_str());
    ptrs.push_back(nullptr);

    if (!filtered_.empty()) {
        XawListChange(listWidget_,
            const_cast<String*>(ptrs.data()),
            (int)filtered_.size(),
            0, True);
    } else {
        static const char* empty[] = {"(no results)", nullptr};
        XawListChange(listWidget_,
            const_cast<String*>(empty), 1, 0, True);
    }
}

// ── show ─────────────────────────────────────────────────────────────────────

void PluginDialog::show(ConfirmCb cb) {
    confirmCb_ = std::move(cb);
    done_ = false;
    XtPopup(shell_, XtGrabExclusive);
    XFlush(app_.display());

    // Nested event loop (modal)
    while (!done_) {
        XEvent ev;
        XtAppNextEvent(app_.appContext(), &ev);
        XtDispatchEvent(&ev);
    }
}

// ── Xt callbacks ──────────────────────────────────────────────────────────────

/*static*/
void PluginDialog::searchKeyHandler(Widget, XtPointer client,
                                    XEvent*, Boolean*) {
    PluginDialog* self = static_cast<PluginDialog*>(client);
    // Use a WorkProc to let the text widget update first
    XtAppAddWorkProc(self->app_.appContext(), lazyFilter, self);
}

/*static*/
Boolean PluginDialog::lazyFilter(XtPointer client) {
    PluginDialog* self = static_cast<PluginDialog*>(client);
    char* val = nullptr;
    XtVaGetValues(self->searchWidget_, XtNstring, &val, nullptr);
    self->rebuildList(val ? val : "");
    return True;  // one-shot work proc
}

/*static*/
void PluginDialog::confirmCB(Widget, XtPointer client, XtPointer) {
    static_cast<PluginDialog*>(client)->onConfirm();
}

/*static*/
void PluginDialog::cancelCB(Widget, XtPointer client, XtPointer) {
    static_cast<PluginDialog*>(client)->onCancel();
}

void PluginDialog::onConfirm() {
    XawListReturnStruct* sel = XawListShowCurrent(listWidget_);
    if (sel && sel->list_index >= 0 && sel->list_index < (int)filtered_.size()) {
        const std::string uri = filtered_[(size_t)sel->list_index].uri;
        done_ = true;
        XtPopdown(shell_);
        if (confirmCb_) confirmCb_(uri);
        // Caller is responsible for deleting this dialog
    }
}

void PluginDialog::onCancel() {
    done_ = true;
    XtPopdown(shell_);
}
