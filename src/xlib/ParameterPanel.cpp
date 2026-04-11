// src/xlib/ParameterPanel.cpp — Xlib/Xaw port
// Dynamic panel of per-port controls for one LV2 plugin slot.

#include "ParameterPanel.h"

#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Dialog.h>

#include <cstdio>
#include <cstring>
#include <cmath>

// ── Constructor ───────────────────────────────────────────────────────────────

ParameterPanel::ParameterPanel(Widget parent, XlibApp& app,
                               SetValCb valCb, SetFileCb fileCb)
    : app_(app), valCb_(std::move(valCb)), fileCb_(std::move(fileCb)) {
    box_ = XtVaCreateManagedWidget("paramBox",
        boxWidgetClass, parent,
        XtNorientation, XtorientVertical,
        XtNborderWidth, 0,
        nullptr);
}

// ── clear ─────────────────────────────────────────────────────────────────────

void ParameterPanel::clear() {
    // Destroy all row widgets (this recursively destroys children)
    for (Widget w : rowWidgets_)
        if (w) XtDestroyWidget(w);
    rowWidgets_.clear();

    // Explicitly destroy popup menus (they are not children of rows)
    for (Widget w : menuWidgets_)
        if (w) XtDestroyWidget(w);
    menuWidgets_.clear();

    for (auto* p : floatItems_)   delete p;
    for (auto* p : toggleItems_)  delete p;
    for (auto* p : triggerItems_) delete p;
    for (auto* p : enumItems_)    delete p;
    for (auto* p : enumItemItems_)delete p;
    for (auto* p : fileItems_)    delete p;

    floatItems_.clear();
    toggleItems_.clear();
    triggerItems_.clear();
    enumItems_.clear();
    enumItemItems_.clear();
    fileItems_.clear();
}

// ── populate ──────────────────────────────────────────────────────────────────

void ParameterPanel::populate(const std::vector<PortDef>& ports) {
    clear();
    for (const auto& p : ports) {
        switch (p.kind) {
        case PortKind::Float:       addFloatControl(p);   break;
        case PortKind::Toggle:      addToggleControl(p);  break;
        case PortKind::Trigger:     addTriggerControl(p); break;
        case PortKind::Enum:        addEnumControl(p);    break;
        case PortKind::AtomFilePath:addFileControl(p);    break;
        }
    }
}

// ── Float (scrollbar slider) ──────────────────────────────────────────────────

void ParameterPanel::addFloatControl(const PortDef& p) {
    Widget row = XtVaCreateManagedWidget("paramRow",
        boxWidgetClass, box_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);
    rowWidgets_.push_back(row);

    XtVaCreateManagedWidget(p.name.c_str(),
        labelWidgetClass, row,
        XtNwidth,       150,
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);

    Widget slider = XtVaCreateManagedWidget("slider",
        scrollbarWidgetClass, row,
        XtNorientation, XtorientHorizontal,
        XtNwidth,       180,
        XtNheight,      20,
        XtNthickness,   10,
        nullptr);

    auto* d = new FloatData{this, p.index, p.min, p.max, p.def};
    floatItems_.push_back(d);

    float range = p.max - p.min;
    float initial = (range > 0.0f)
                    ? (p.def - p.min) / range
                    : 0.5f;
    if (initial < 0.0f) initial = 0.0f;
    if (initial > 1.0f) initial = 1.0f;
    XawScrollbarSetThumb(slider, initial, 0.04f);

    d->current = p.def;

    XtAddCallback(slider, XtNjumpProc,
        [](Widget, XtPointer client, XtPointer call) {
            FloatData* fd = static_cast<FloatData*>(client);
            float pos = *reinterpret_cast<float*>(call);
            if (pos < 0.0f) pos = 0.0f;
            if (pos > 1.0f) pos = 1.0f;
            fd->current = fd->min + pos * (fd->max - fd->min);
            if (fd->panel->valCb_)
                fd->panel->valCb_(fd->portIndex, fd->current);
        }, d);
    XtAddCallback(slider, XtNscrollProc,
        [](Widget w, XtPointer client, XtPointer call) {
            FloatData* fd = static_cast<FloatData*>(client);
            int delta = (int)(intptr_t)call;
            float step = (fd->max - fd->min) * 0.01f * (delta > 0 ? 1.0f : -1.0f);
            fd->current += step;
            if (fd->current < fd->min) fd->current = fd->min;
            if (fd->current > fd->max) fd->current = fd->max;
            float pos = (fd->max - fd->min > 0.0f)
                        ? (fd->current - fd->min) / (fd->max - fd->min)
                        : 0.5f;
            XawScrollbarSetThumb(w, pos, 0.04f);
            if (fd->panel->valCb_)
                fd->panel->valCb_(fd->portIndex, fd->current);
        }, d);

    // Value display label
    char vbuf[24];
    snprintf(vbuf, sizeof(vbuf), "%.3g", (double)p.def);
    XtVaCreateManagedWidget(vbuf,
        labelWidgetClass, row,
        XtNwidth,       60,
        XtNborderWidth, 0,
        nullptr);
}

// ── Toggle (check button) ─────────────────────────────────────────────────────

void ParameterPanel::addToggleControl(const PortDef& p) {
    Widget row = XtVaCreateManagedWidget("paramRow",
        boxWidgetClass, box_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);
    rowWidgets_.push_back(row);

    XtVaCreateManagedWidget(p.name.c_str(),
        labelWidgetClass, row,
        XtNwidth,       150,
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);

    auto* d = new ToggleData{this, p.index, p.max, p.min};
    toggleItems_.push_back(d);

    Widget toggle = XtVaCreateManagedWidget("toggle",
        toggleWidgetClass, row,
        XtNstate, (p.def > 0.5f) ? True : False,
        nullptr);
    XtAddCallback(toggle, XtNcallback,
        [](Widget w, XtPointer client, XtPointer) {
            ToggleData* td = static_cast<ToggleData*>(client);
            Boolean on = False;
            XtVaGetValues(w, XtNstate, &on, nullptr);
            if (td->panel->valCb_)
                td->panel->valCb_(td->portIndex,
                    on ? td->onValue : td->offValue);
        }, d);
}

// ── Trigger (fire button) ─────────────────────────────────────────────────────

void ParameterPanel::addTriggerControl(const PortDef& p) {
    Widget row = XtVaCreateManagedWidget("paramRow",
        boxWidgetClass, box_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);
    rowWidgets_.push_back(row);

    XtVaCreateManagedWidget(p.name.c_str(),
        labelWidgetClass, row,
        XtNwidth,       150,
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);

    auto* d = new TriggerData{this, p.index, 1.0f};
    triggerItems_.push_back(d);

    Widget btn = XtVaCreateManagedWidget("Fire", commandWidgetClass, row, nullptr);
    XtAddCallback(btn, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            TriggerData* td = static_cast<TriggerData*>(client);
            if (td->panel->valCb_)
                td->panel->valCb_(td->portIndex, td->onValue);
        }, d);
}

// ── Enum (popup menu) ─────────────────────────────────────────────────────────

void ParameterPanel::addEnumControl(const PortDef& p) {
    Widget row = XtVaCreateManagedWidget("paramRow",
        boxWidgetClass, box_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);
    rowWidgets_.push_back(row);

    XtVaCreateManagedWidget(p.name.c_str(),
        labelWidgetClass, row,
        XtNwidth,       150,
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);

    auto* ed = new EnumData{this, p.index, p.def, 0};
    enumItems_.push_back(ed);

    // Unique menu name per port to avoid popup conflicts
    char menuName[64];
    snprintf(menuName, sizeof(menuName), "enumMenu_%u", p.index);

    const std::string& firstLabel = p.options.empty() ? "?" : p.options[0];
    Widget btn = XtVaCreateManagedWidget(firstLabel.c_str(),
        menuButtonWidgetClass, row,
        XtNmenuName, menuName,
        XtNwidth,    160,
        nullptr);

    // Create popup menu as child of the top-level shell (not the row widget)
    // so it is not clipped inside the viewport.
    Widget menu = XtVaCreatePopupShell(menuName,
        simpleMenuWidgetClass, btn, nullptr);
    menuWidgets_.push_back(menu);

    for (int i = 0; i < (int)p.options.size(); ++i) {
        auto* item = new EnumItemData{this, p.index, p.def, i, btn};
        // For enum scale points we only have names here; original float values
        // would need to be passed by populate() — for now use index as value.
        item->value = p.min + i * ((p.max - p.min) / std::max((int)p.options.size() - 1, 1));
        enumItemItems_.push_back(item);

        Widget menuItem = XtVaCreateManagedWidget(p.options[(size_t)i].c_str(),
            smeBSBObjectClass, menu, nullptr);
        XtAddCallback(menuItem, XtNcallback,
            [](Widget, XtPointer client, XtPointer) {
                EnumItemData* ei = static_cast<EnumItemData*>(client);
                // Update the button label
                XtVaSetValues(ei->menuBtn, XtNlabel,
                    XtName(XtParent(ei->menuBtn)), // workaround: just use index
                    nullptr);
                if (ei->panel->valCb_)
                    ei->panel->valCb_(ei->portIndex, ei->value);
            }, item);
    }
}

// ── AtomFilePath (browse button) ─────────────────────────────────────────────

void ParameterPanel::addFileControl(const PortDef& p) {
    Widget row = XtVaCreateManagedWidget("paramRow",
        boxWidgetClass, box_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);
    rowWidgets_.push_back(row);

    XtVaCreateManagedWidget(p.name.c_str(),
        labelWidgetClass, row,
        XtNwidth,       150,
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);

    auto* d = new FileData{this, p.index};
    fileItems_.push_back(d);

    Widget btn = XtVaCreateManagedWidget("Browse…", commandWidgetClass, row, nullptr);
    XtAddCallback(btn, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            FileData* fd = static_cast<FileData*>(client);
            // Show a simple text-entry dialog for the file path.
            // We reuse the inline SimpleFileDlg pattern from MainWindow.cpp
            // by delegating through the fileCb_. The UI code for the dialog
            // lives in MainWindow's namespace; here we fire a callback that
            // includes a path request via fd->panel->app_.

            // Build a small nested-loop text dialog
            XlibApp& myApp = fd->panel->app_;
            Widget topLevel = myApp.topLevel();

            // SimpleFileDlg (same pattern as MainWindow.cpp)
            struct Dlg {
                XtAppContext ctx;
                Widget textW;
                std::string result;
                bool done = false, ok = false;
            };
            Dlg d2;
            d2.ctx = myApp.appContext();

            Widget sh = XtVaCreatePopupShell("fileChooser",
                transientShellWidgetClass, topLevel,
                XtNtitle, "Select file",
                nullptr);
            Widget frm = XtVaCreateManagedWidget("f", formWidgetClass, sh,
                XtNdefaultDistance, 6, nullptr);
            Widget lbl = XtVaCreateManagedWidget("Path:", labelWidgetClass, frm,
                XtNborderWidth, 0,
                XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr);
            d2.textW = XtVaCreateManagedWidget("t", asciiTextWidgetClass, frm,
                XtNwidth, 360, XtNeditType, XawtextEdit,
                XtNfromVert, lbl,
                XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr);

            // OK button
            XtAddCallback(
                XtVaCreateManagedWidget("OK", commandWidgetClass, frm,
                    XtNfromVert, d2.textW,
                    XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr),
                XtNcallback,
                [](Widget, XtPointer c, XtPointer) {
                    Dlg* dd = (Dlg*)c;
                    char* v = nullptr;
                    XtVaGetValues(dd->textW, XtNstring, &v, nullptr);
                    dd->result = v ? v : "";
                    dd->ok   = true;
                    dd->done = true;
                }, &d2);

            // Cancel button
            Widget okw = XtNameToWidget(frm, "OK");
            XtAddCallback(
                XtVaCreateManagedWidget("Cancel", commandWidgetClass, frm,
                    XtNfromVert, d2.textW,
                    XtNfromHoriz, okw,
                    XtNtop, XawChainTop, XtNleft, XawChainLeft, nullptr),
                XtNcallback,
                [](Widget, XtPointer c, XtPointer) {
                    ((Dlg*)c)->done = true;
                }, &d2);

            XtPopup(sh, XtGrabExclusive);
            XFlush(myApp.display());
            while (!d2.done) {
                XEvent ev;
                XtAppNextEvent(d2.ctx, &ev);
                XtDispatchEvent(&ev);
            }
            XtDestroyWidget(sh);

            if (d2.ok && !d2.result.empty() && fd->panel->fileCb_)
                fd->panel->fileCb_(fd->portIndex, d2.result);
        }, d);
}
