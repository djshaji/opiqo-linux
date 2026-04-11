// src/xlib/SettingsDialog.cpp — Xlib/Xaw port

#include "SettingsDialog.h"
#include "logging_macros.h"

#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xmu/Atoms.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <libgen.h>
#include <unistd.h>

// ── Constructor / destructor ──────────────────────────────────────────────────

SettingsDialog::SettingsDialog(Widget parent, XlibApp& app,
                               const AppSettings& current,
                               const std::vector<PortInfo>& cap,
                               const std::vector<PortInfo>& pb)
    : app_(app), parent_(parent), current_(current),
      capturePorts_(cap), playbackPorts_(pb) {
    buildWidgets();
}

SettingsDialog::~SettingsDialog() {
    for (auto* p : portItems_) delete p;
    if (shell_) XtDestroyWidget(shell_);
}

// ── show / hide ───────────────────────────────────────────────────────────────

void SettingsDialog::show() {
    visible_ = true;
    XtPopup(shell_, XtGrabNonexclusive);
    XRaiseWindow(app_.display(), XtWindow(shell_));
}

void SettingsDialog::hide() {
    visible_ = false;
    XtPopdown(shell_);
}

// ── updatePorts ───────────────────────────────────────────────────────────────

void SettingsDialog::updatePorts(const std::vector<PortInfo>& cap,
                                 const std::vector<PortInfo>& pb) {
    capturePorts_  = cap;
    playbackPorts_ = pb;
    // Rebuild port dropdowns
    rebuildPortDropdown(cap1Btn_, &cap1Menu_, capturePorts_,
                        current_.capturePort,  &cap1Items_);
    rebuildPortDropdown(cap2Btn_, &cap2Menu_, capturePorts_,
                        current_.capturePort2, &cap2Items_);
    rebuildPortDropdown(pb1Btn_,  &pb1Menu_,  playbackPorts_,
                        current_.playbackPort,  &pb1Items_);
    rebuildPortDropdown(pb2Btn_,  &pb2Menu_,  playbackPorts_,
                        current_.playbackPort2, &pb2Items_);
}

// ── GetSettings ──────────────────────────────────────────────────────────────

AppSettings SettingsDialog::getSettings() const {
    AppSettings s = current_;
    // Read current port selection from MenuButton labels (we track by index)
    auto portId = [&](Widget btn,
                      const std::vector<PortInfo>& ports,
                      const std::vector<int>& /*items*/) -> std::string {
        char* lbl = nullptr;
        XtVaGetValues(btn, XtNlabel, &lbl, nullptr);
        // Find matching PortInfo by friendly name
        if (lbl) {
            for (const auto& p : ports)
                if (p.friendlyName == lbl) return p.id;
        }
        return ports.empty() ? "" : ports[0].id;
    };

    s.capturePort  = portId(cap1Btn_, capturePorts_,  cap1Items_);
    s.capturePort2 = portId(cap2Btn_, capturePorts_,  cap2Items_);
    s.playbackPort = portId(pb1Btn_,  playbackPorts_, pb1Items_);
    s.playbackPort2= portId(pb2Btn_,  playbackPorts_, pb2Items_);
    return s;
}

// ── Widget construction ───────────────────────────────────────────────────────

void SettingsDialog::buildWidgets() {
    shell_ = XtVaCreatePopupShell("settings",
        transientShellWidgetClass, parent_,
        XtNtitle, "Settings",
        XtNwidth,  480,
        XtNheight, 360,
        nullptr);

    // WM_DELETE_WINDOW protocol
    Atom wm_del = XInternAtom(app_.display(), "WM_DELETE_WINDOW", False);
    XSetWMProtocols(app_.display(), XtWindow(shell_), &wm_del, 1);
    XtAddEventHandler(shell_, NoEventMask, True, wmDeleteHandler, this);

    Widget mainForm = XtVaCreateManagedWidget("mainForm",
        formWidgetClass, shell_,
        XtNdefaultDistance, 6,
        nullptr);

    // ── Tab strip ─────────────────────────────────────────────────────────
    Widget tabBox = XtVaCreateManagedWidget("tabs",
        boxWidgetClass, mainForm,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        XtNtop,    XawChainTop,
        XtNleft,   XawChainLeft,
        XtNright,  XawChainRight,
        nullptr);

    tabAudioBtn_ = XtVaCreateManagedWidget("Audio",
        toggleWidgetClass, tabBox,
        XtNstate, True,
        nullptr);
    XtAddCallback(tabAudioBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<SettingsDialog*>(client)->switchTab(0);
        }, this);

    tabPresetsBtn_ = XtVaCreateManagedWidget("Presets",
        toggleWidgetClass, tabBox,
        XtNstate, False,
        nullptr);
    XtAddCallback(tabPresetsBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<SettingsDialog*>(client)->switchTab(1);
        }, this);

    // ── Audio tab ─────────────────────────────────────────────────────────
    audioTab_ = XtVaCreateManagedWidget("audioTab",
        formWidgetClass, mainForm,
        XtNdefaultDistance, 6,
        XtNfromVert, tabBox,
        XtNtop,    XawRubber,
        XtNleft,   XawChainLeft,
        XtNright,  XawChainRight,
        XtNbottom, XawRubber,
        nullptr);

    buildAudioTab();

    // ── Presets tab ───────────────────────────────────────────────────────
    presetsTab_ = XtVaCreateWidget("presetsTab",
        formWidgetClass, mainForm,
        XtNdefaultDistance, 6,
        XtNfromVert, tabBox,
        XtNtop,    XawRubber,
        XtNleft,   XawChainLeft,
        XtNright,  XawChainRight,
        XtNbottom, XawRubber,
        nullptr);

    buildPresetsTab();

    // ── Close button ─────────────────────────────────────────────────────
    Widget closeBtn = XtVaCreateManagedWidget("Close",
        commandWidgetClass, mainForm,
        XtNfromVert, audioTab_,
        XtNtop,    XawChainBottom,
        XtNleft,   XawChainLeft,
        XtNbottom, XawChainBottom,
        nullptr);
    XtAddCallback(closeBtn, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<SettingsDialog*>(client)->hide();
        }, this);

    activeTab_ = 0;
}

void SettingsDialog::buildAudioTab() {
    // Helper: add a label + control pair as successive rows using fromVert
    auto addRow = [&](Widget& prev, const char* labelText,
                      Widget ctrl) -> Widget {
        Widget lbl = XtVaCreateManagedWidget(labelText,
            labelWidgetClass, audioTab_,
            XtNborderWidth, 0,
            XtNjustify,     XtJustifyRight,
            XtNwidth,       100,
            XtNfromVert, prev,
            XtNtop,      XawChainTop,
            XtNleft,     XawChainLeft,
            nullptr);
        XtVaSetValues(ctrl,
            XtNfromHoriz, lbl,
            XtNfromVert,  prev,
            XtNtop,      XawChainTop,
            XtNleft,     XawChainLeft,
            nullptr);
        prev = lbl;
        return lbl;
    };

    Widget prev = nullptr;

    // Capture L
    buildPortDropdown(audioTab_, &cap1Btn_, &cap1Menu_,
                      capturePorts_, current_.capturePort, &cap1Items_);
    addRow(prev, "Capture L:", cap1Btn_);

    // Capture R
    buildPortDropdown(audioTab_, &cap2Btn_, &cap2Menu_,
                      capturePorts_, current_.capturePort2, &cap2Items_);
    addRow(prev, "Capture R:", cap2Btn_);

    // Playback L
    buildPortDropdown(audioTab_, &pb1Btn_, &pb1Menu_,
                      playbackPorts_, current_.playbackPort, &pb1Items_);
    addRow(prev, "Playback L:", pb1Btn_);

    // Playback R
    buildPortDropdown(audioTab_, &pb2Btn_, &pb2Menu_,
                      playbackPorts_, current_.playbackPort2, &pb2Items_);
    addRow(prev, "Playback R:", pb2Btn_);

    // Sample rate / block size (read-only labels)
    srLabel_ = XtVaCreateWidget("srLbl",
        labelWidgetClass, audioTab_,
        XtNlabel, "—",
        XtNborderWidth, 0,
        nullptr);
    addRow(prev, "Sample rate:", srLabel_);

    bsLabel_ = XtVaCreateWidget("bsLbl",
        labelWidgetClass, audioTab_,
        XtNlabel, "—",
        XtNborderWidth, 0,
        nullptr);
    addRow(prev, "Block size:", bsLabel_);

    // Apply button
    applyBtn_ = XtVaCreateManagedWidget("Apply",
        commandWidgetClass, audioTab_,
        XtNfromVert, prev,
        XtNtop,      XawChainTop,
        XtNleft,     XawChainLeft,
        nullptr);
    XtAddCallback(applyBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            SettingsDialog* self = static_cast<SettingsDialog*>(client);
            if (self->applyCb_) self->applyCb_(self->getSettings());
        }, this);

    // Delete plugin cache button
    delCacheBtn_ = XtVaCreateManagedWidget("Delete Plugin Cache",
        commandWidgetClass, audioTab_,
        XtNfromVert, applyBtn_,
        XtNtop,      XawChainTop,
        XtNleft,     XawChainLeft,
        nullptr);
    XtAddCallback(delCacheBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            SettingsDialog* self = static_cast<SettingsDialog*>(client);
            if (self->cacheCb_) self->cacheCb_();
        }, this);
}

void SettingsDialog::buildPresetsTab() {
    Widget desc = XtVaCreateManagedWidget(
        "Export or import the current 4-slot preset.",
        labelWidgetClass, presetsTab_,
        XtNborderWidth, 0,
        XtNjustify, XtJustifyLeft,
        XtNtop,  XawChainTop,
        XtNleft, XawChainLeft,
        nullptr);

    exportBtn_ = XtVaCreateManagedWidget("Export Preset…",
        commandWidgetClass, presetsTab_,
        XtNfromVert, desc,
        XtNtop, XawChainTop, XtNleft, XawChainLeft,
        nullptr);
    XtAddCallback(exportBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            SettingsDialog* self = static_cast<SettingsDialog*>(client);
            if (self->exportCb_) self->exportCb_();
        }, this);

    importBtn_ = XtVaCreateManagedWidget("Import Preset…",
        commandWidgetClass, presetsTab_,
        XtNfromVert, exportBtn_,
        XtNtop, XawChainTop, XtNleft, XawChainLeft,
        nullptr);
    XtAddCallback(importBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            SettingsDialog* self = static_cast<SettingsDialog*>(client);
            if (!self->importCb_) return;

            // Ask for file path via simple inline dialog
            Widget sh = XtVaCreatePopupShell("importDlg",
                transientShellWidgetClass, self->shell_,
                XtNtitle, "Import — enter file path", nullptr);
            Widget f = XtVaCreateManagedWidget("f", formWidgetClass, sh,
                XtNdefaultDistance, 6, nullptr);
            Widget lbl = XtVaCreateManagedWidget("Path:", labelWidgetClass, f,
                XtNborderWidth, 0,
                XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr);
            Widget txt = XtVaCreateManagedWidget("t", asciiTextWidgetClass, f,
                XtNwidth, 360, XtNeditType, XawtextEdit,
                XtNfromVert, lbl,
                XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr);

            struct Dlg { XtAppContext ctx; Widget txt;
                         SettingsDialog* sd; Widget sh; bool done; };
            Dlg* dd = new Dlg{self->app_.appContext(), txt, self, sh, false};

            auto okFn = [](Widget, XtPointer c, XtPointer) {
                Dlg* d = (Dlg*)c;
                char* v = nullptr;
                XtVaGetValues(d->txt, XtNstring, &v, nullptr);
                std::string path = v ? v : "";
                d->done = true;
                XtDestroyWidget(d->sh);
                if (!path.empty() && d->sd->importCb_) d->sd->importCb_(path);
                delete d;
            };
            auto cancelFn = [](Widget, XtPointer c, XtPointer) {
                Dlg* d = (Dlg*)c;
                d->done = true;
                XtDestroyWidget(d->sh);
                delete d;
            };

            Widget okw = XtVaCreateManagedWidget("OK", commandWidgetClass, f,
                XtNfromVert, txt,
                XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr);
            XtAddCallback(okw, XtNcallback, okFn, dd);

            XtAddCallback(
                XtVaCreateManagedWidget("Cancel", commandWidgetClass, f,
                    XtNfromVert, txt, XtNfromHoriz, okw,
                    XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr),
                XtNcallback, cancelFn, dd);

            XtPopup(sh, XtGrabExclusive);
        }, this);
}

// ── Port dropdown helpers ─────────────────────────────────────────────────────

void SettingsDialog::buildPortDropdown(Widget parent,
                                       Widget* btn, Widget* menu,
                                       const std::vector<PortInfo>& ports,
                                       const std::string& selected,
                                       std::vector<int>* /*itemStorage*/) {
    const std::string& initLabel = ports.empty() ? "(none)"
        : [&]() -> const std::string& {
              for (const auto& p : ports)
                  if (p.id == selected) return p.friendlyName;
              return ports[0].friendlyName;
          }();

    char menuName[64];
    static int counter = 0;
    snprintf(menuName, sizeof(menuName), "portMenu%d", ++counter);

    *btn = XtVaCreateManagedWidget(initLabel.c_str(),
        menuButtonWidgetClass, parent,
        XtNmenuName, menuName,
        XtNwidth, 200,
        nullptr);

    *menu = XtVaCreatePopupShell(menuName,
        simpleMenuWidgetClass, *btn, nullptr);

    for (int i = 0; i < (int)ports.size(); ++i) {
        struct Item { SettingsDialog* dlg; Widget btn;
                      const PortInfo* port; };
        auto* it = new Item{this, *btn, &ports[(size_t)i]};
        portItems_.push_back(reinterpret_cast<PortItem*>(it));
        Widget entry = XtVaCreateManagedWidget(ports[(size_t)i].friendlyName.c_str(),
            smeBSBObjectClass, *menu, nullptr);
        XtAddCallback(entry, XtNcallback,
            [](Widget, XtPointer client, XtPointer) {
                Item* item = static_cast<Item*>(client);
                XtVaSetValues(item->btn,
                    XtNlabel, item->port->friendlyName.c_str(), nullptr);
            }, it);
    }
}

void SettingsDialog::rebuildPortDropdown(Widget btn, Widget* menu,
                                          const std::vector<PortInfo>& ports,
                                          const std::string& selected,
                                          std::vector<int>* /*items*/) {
    if (*menu) { XtDestroyWidget(*menu); *menu = nullptr; }

    char menuName[64];
    static int counter = 100;
    snprintf(menuName, sizeof(menuName), "portMenu%d", ++counter);

    XtVaSetValues(btn, XtNmenuName, menuName, nullptr);
    *menu = XtVaCreatePopupShell(menuName,
        simpleMenuWidgetClass, btn, nullptr);

    for (int i = 0; i < (int)ports.size(); ++i) {
        struct Item { Widget btn; const PortInfo* port; };
        auto* it = new Item{btn, &ports[(size_t)i]};
        Widget entry = XtVaCreateManagedWidget(ports[(size_t)i].friendlyName.c_str(),
            smeBSBObjectClass, *menu, nullptr);
        XtAddCallback(entry, XtNcallback,
            [](Widget, XtPointer client, XtPointer) {
                Item* item = static_cast<Item*>(client);
                XtVaSetValues(item->btn,
                    XtNlabel, item->port->friendlyName.c_str(), nullptr);
                delete item;
            }, it);
    }

    // Set initial label
    const std::string& initLabel = ports.empty() ? "(none)"
        : [&]() -> const std::string& {
              for (const auto& p : ports)
                  if (p.id == selected) return p.friendlyName;
              return ports[0].friendlyName;
          }();
    XtVaSetValues(btn, XtNlabel, initLabel.c_str(), nullptr);
}

// ── Tab switching ─────────────────────────────────────────────────────────────

void SettingsDialog::switchTab(int idx) {
    activeTab_ = idx;
    if (idx == 0) {
        XtManageChild(audioTab_);
        XtUnmanageChild(presetsTab_);
        XtVaSetValues(tabAudioBtn_,   XtNstate, True,  nullptr);
        XtVaSetValues(tabPresetsBtn_, XtNstate, False, nullptr);
    } else {
        XtUnmanageChild(audioTab_);
        XtManageChild(presetsTab_);
        XtVaSetValues(tabAudioBtn_,   XtNstate, False, nullptr);
        XtVaSetValues(tabPresetsBtn_, XtNstate, True,  nullptr);
    }
}

// ── WM_DELETE_WINDOW ──────────────────────────────────────────────────────────

/*static*/
void SettingsDialog::wmDeleteHandler(Widget, XtPointer client,
                                     XEvent* ev, Boolean*) {
    if (ev->type == ClientMessage) {
        Atom wm_del = XInternAtom(((SettingsDialog*)client)->app_.display(),
                                  "WM_DELETE_WINDOW", False);
        if ((Atom)ev->xclient.data.l[0] == wm_del)
            static_cast<SettingsDialog*>(client)->hide();
    }
}
