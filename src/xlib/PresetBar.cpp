// src/xlib/PresetBar.cpp — Xlib/Xaw port

#include "PresetBar.h"

#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/AsciiText.h>
#include <cstring>
#include <algorithm>

// ── Constructor / destructor ──────────────────────────────────────────────────

PresetBar::PresetBar(Widget parent, XlibApp& app) : app_(app) {
    buildWidgets(parent);
}

PresetBar::~PresetBar() {
    for (auto* p : presetItems_) delete p;
}

// ── Widget construction ───────────────────────────────────────────────────────

void PresetBar::buildWidgets(Widget parent) {
    bar_ = XtVaCreateManagedWidget("presetBar",
        boxWidgetClass, parent,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);

    XtVaCreateManagedWidget("prsLbl", labelWidgetClass, bar_,
        XtNlabel, " Presets:", XtNborderWidth, 0, nullptr);

    // MenuButton showing current preset name
    presetBtn_ = XtVaCreateManagedWidget("presetSelect",
        menuButtonWidgetClass, bar_,
        XtNmenuName, "presetPopup",
        XtNlabel,    "<none>",
        XtNwidth,    160,
        nullptr);
    // Start with an empty menu (rebuilt by setPresetNames)
    presetMenu_ = XtVaCreatePopupShell("presetPopup",
        simpleMenuWidgetClass, presetBtn_, nullptr);

    // Name text entry
    nameEntry_ = XtVaCreateManagedWidget("presetName",
        asciiTextWidgetClass, bar_,
        XtNwidth,    160,
        XtNeditType, XawtextEdit,
        XtNstring,   "",
        nullptr);

    // Load / Save / Delete buttons
    loadBtn_ = XtVaCreateManagedWidget("Load", commandWidgetClass, bar_, nullptr);
    XtAddCallback(loadBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            PresetBar* self = static_cast<PresetBar*>(client);
            if (self->loadCb_) self->loadCb_();
        }, this);
    // Note: The above lambda returns bool; use a proper void lambda:

    saveBtn_ = XtVaCreateManagedWidget("Save", commandWidgetClass, bar_, nullptr);
    XtAddCallback(saveBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            PresetBar* self = static_cast<PresetBar*>(client);
            if (self->saveCb_) {
                char* val = nullptr;
                XtVaGetValues(self->nameEntry_, XtNstring, &val, nullptr);
                self->saveCb_(val ? std::string(val) : "");
            }
        }, this);

    deleteBtn_ = XtVaCreateManagedWidget("Delete", commandWidgetClass, bar_, nullptr);
    XtAddCallback(deleteBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            PresetBar* self = static_cast<PresetBar*>(client);
            if (self->deleteCb_) self->deleteCb_();
        }, this);
}

// ── Preset menu management ────────────────────────────────────────────────────

void PresetBar::destroyPresetMenu() {
    if (presetMenu_) {
        XtDestroyWidget(presetMenu_);
        presetMenu_ = nullptr;
    }
    for (auto* p : presetItems_) delete p;
    presetItems_.clear();
}

void PresetBar::buildPresetMenu() {
    presetMenu_ = XtVaCreatePopupShell("presetPopup",
        simpleMenuWidgetClass, presetBtn_, nullptr);

    for (int i = 0; i < (int)presetNames_.size(); ++i) {
        auto* d = new PresetItem{this, i};
        presetItems_.push_back(d);
        Widget item = XtVaCreateManagedWidget(presetNames_[(size_t)i].c_str(),
            smeBSBObjectClass, presetMenu_, nullptr);
        XtAddCallback(item, XtNcallback,
            [](Widget, XtPointer client, XtPointer) {
                PresetItem* pi = static_cast<PresetItem*>(client);
                pi->bar->onPresetSelected(pi->index);
            }, d);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void PresetBar::setPresetNames(const std::vector<std::string>& names) {
    presetNames_ = names;
    destroyPresetMenu();
    buildPresetMenu();
    selectedIndex_ = -1;
    XtVaSetValues(presetBtn_, XtNlabel, "<none>", nullptr);
}

void PresetBar::setCurrentName(const std::string& name) {
    // Update the AsciiText entry
    XtVaSetValues(nameEntry_, XtNstring, name.c_str(), nullptr);
    // Update the MenuButton label
    XtVaSetValues(presetBtn_, XtNlabel,
        name.empty() ? "<none>" : name.c_str(), nullptr);
}

std::string PresetBar::getCurrentName() const {
    char* val = nullptr;
    XtVaGetValues(nameEntry_, XtNstring, &val, nullptr);
    return val ? std::string(val) : "";
}

void PresetBar::onPresetSelected(int idx) {
    selectedIndex_ = idx;
    if (idx >= 0 && idx < (int)presetNames_.size()) {
        const std::string& name = presetNames_[(size_t)idx];
        XtVaSetValues(presetBtn_, XtNlabel, name.c_str(), nullptr);
        XtVaSetValues(nameEntry_, XtNstring, name.c_str(), nullptr);
    }
}
