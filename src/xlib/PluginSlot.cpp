// src/xlib/PluginSlot.cpp — Xlib/Xaw port

#include "PluginSlot.h"
#include "ParameterPanel.h"

#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Viewport.h>

#include <cstdio>

// ── Constructor / destructor ──────────────────────────────────────────────────

PluginSlot::PluginSlot(int slot, Widget parent, XlibApp& app)
    : slot_(slot), app_(app) {
    buildWidgets(parent);
}

PluginSlot::~PluginSlot() {
    delete params_;
}

// ── Widget construction ───────────────────────────────────────────────────────

void PluginSlot::buildWidgets(Widget parent) {
    char slotLabel[32];
    std::snprintf(slotLabel, sizeof(slotLabel), "Slot %d", slot_);

    // ── Outer form (acts as a bordered frame) ─────────────────────────────
    frame_ = XtVaCreateWidget("slotFrame",
        formWidgetClass, parent,
        XtNborderWidth, 2,
        XtNdefaultDistance, 4,
        nullptr);

    // ── Header row ────────────────────────────────────────────────────────
    headerBox_ = XtVaCreateManagedWidget("slotHeader",
        boxWidgetClass, frame_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        XtNtop,     XawChainTop,
        XtNleft,    XawChainLeft,
        XtNright,   XawChainRight,
        nullptr);

    nameLabel_ = XtVaCreateManagedWidget("slotName",
        labelWidgetClass, headerBox_,
        XtNlabel,       slotLabel,
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);

    addBtn_ = XtVaCreateManagedWidget("+ Add",
        commandWidgetClass, headerBox_, nullptr);
    XtAddCallback(addBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            PluginSlot* s = static_cast<PluginSlot*>(client);
            if (s->addCb_) s->addCb_(s->slot_);
        }, this);

    bypassToggle_ = XtVaCreateManagedWidget("Bypass",
        toggleWidgetClass, headerBox_,
        XtNsensitive, False,
        nullptr);
    XtAddCallback(bypassToggle_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            PluginSlot* s = static_cast<PluginSlot*>(client);
            Boolean state = False;
            XtVaGetValues(s->bypassToggle_, XtNstate, &state, nullptr);
            if (s->bypassCb_) s->bypassCb_(s->slot_, state == True);
        }, this);

    deleteBtn_ = XtVaCreateManagedWidget("x Remove",
        commandWidgetClass, headerBox_,
        XtNsensitive,    False,
        XtNforeground,   app_.colors().red,
        nullptr);
    XtAddCallback(deleteBtn_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            PluginSlot* s = static_cast<PluginSlot*>(client);
            if (s->deleteCb_) s->deleteCb_(s->slot_);
        }, this);

    // ── Scrollable parameter area ─────────────────────────────────────────
    paramArea_ = XtVaCreateManagedWidget("paramArea",
        viewportWidgetClass, frame_,
        XtNfromVert,       headerBox_,
        XtNtop,            XawRubber,
        XtNleft,           XawChainLeft,
        XtNright,          XawChainRight,
        XtNbottom,         XawChainBottom,
        XtNwidth,          300,
        XtNheight,         200,
        XtNallowVert,      True,
        nullptr);

    // ParameterPanel — create inside the viewport
    params_ = new ParameterPanel(paramArea_, app_,
        [this](uint32_t portIdx, float val) {
            if (setValCb_) setValCb_(slot_, portIdx, val);
        },
        [this](uint32_t portIdx, const std::string& path) {
            if (setFileCb_) setFileCb_(slot_, portIdx, path);
        });

    XtManageChild(frame_);
}

// ── Plugin added / cleared ────────────────────────────────────────────────────

void PluginSlot::onPluginAdded(const std::string& name,
                               const std::string& portsJson) {
    // portsJson is not used directly here — MainWindow passes the PortInfo vector
    // via the overloaded version; this string form is kept for API compatibility.
    (void)portsJson;
    XtVaSetValues(nameLabel_, XtNlabel, name.c_str(), nullptr);
    XtVaSetValues(bypassToggle_, XtNsensitive, True, nullptr);
    XtVaSetValues(deleteBtn_,    XtNsensitive, True, nullptr);
    XtVaSetValues(bypassToggle_, XtNstate, False, nullptr);
    pluginLoaded_ = true;
}

void PluginSlot::onPluginAdded(const std::string& name,
                               const std::vector<LV2Plugin::PortInfo>& ports) {
    XtVaSetValues(nameLabel_, XtNlabel, name.c_str(), nullptr);
    XtVaSetValues(bypassToggle_, XtNsensitive, True, nullptr);
    XtVaSetValues(deleteBtn_,    XtNsensitive, True, nullptr);
    XtVaSetValues(bypassToggle_, XtNstate, False, nullptr);
    pluginLoaded_ = true;

    // Convert to ParameterPanel::PortDef
    std::vector<ParameterPanel::PortDef> defs;
    for (const auto& p : ports) {
        ParameterPanel::PortDef d;
        d.index = p.portIndex;
        d.name  = p.label;
        d.min   = p.minVal;
        d.max   = p.maxVal;
        d.def   = p.defVal;

        switch (p.type) {
        case LV2Plugin::PortInfo::ControlType::Float:
            if (p.isEnum && !p.scalePoints.empty()) {
                d.kind = ParameterPanel::PortKind::Enum;
                for (const auto& sp : p.scalePoints)
                    d.options.push_back(sp.second);
            } else {
                d.kind = ParameterPanel::PortKind::Float;
            }
            break;
        case LV2Plugin::PortInfo::ControlType::Toggle:
            d.kind = ParameterPanel::PortKind::Toggle;
            break;
        case LV2Plugin::PortInfo::ControlType::Trigger:
            d.kind = ParameterPanel::PortKind::Trigger;
            break;
        case LV2Plugin::PortInfo::ControlType::AtomFilePath:
            d.kind = ParameterPanel::PortKind::AtomFilePath;
            break;
        }
        defs.push_back(std::move(d));
    }
    params_->populate(defs);
}

void PluginSlot::onPluginCleared() {
    char label[32];
    std::snprintf(label, sizeof(label), "Slot %d", slot_);
    XtVaSetValues(nameLabel_, XtNlabel, label, nullptr);
    XtVaSetValues(bypassToggle_, XtNsensitive, False,  nullptr);
    XtVaSetValues(bypassToggle_, XtNstate,     False, nullptr);
    XtVaSetValues(deleteBtn_,    XtNsensitive, False,  nullptr);
    pluginLoaded_ = false;
    params_->clear();
}

void PluginSlot::setBypassed(bool b) {
    bypassed_ = b;
    XtVaSetValues(bypassToggle_, XtNstate, b ? True : False, nullptr);
}
