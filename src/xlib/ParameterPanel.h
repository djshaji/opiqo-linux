// src/xlib/ParameterPanel.h — Xlib/Xaw port
// Dynamic panel of per-port controls for one plugin slot.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <cstdint>
#include <vector>
#include "XlibApp.h"

class ParameterPanel {
public:
    using SetValCb  = std::function<void(uint32_t portIdx, float value)>;
    using SetFileCb = std::function<void(uint32_t portIdx, const std::string& path)>;

    enum class PortKind { Float, Toggle, Trigger, Enum, AtomFilePath };

    struct PortDef {
        uint32_t index;
        std::string name;
        PortKind kind;
        float min, max, def;
        std::vector<std::string> options;  // for Enum
    };

    ParameterPanel(Widget parent, XlibApp& app,
                   SetValCb valCb, SetFileCb fileCb);
    ~ParameterPanel() = default;

    Widget widget() const { return box_; }

    void populate(const std::vector<PortDef>& ports);
    void clear();

private:
    void addFloatControl   (const PortDef& p);
    void addToggleControl  (const PortDef& p);
    void addTriggerControl (const PortDef& p);
    void addEnumControl    (const PortDef& p);
    void addFileControl    (const PortDef& p);

    XlibApp& app_;
    Widget   box_ = nullptr;  // Box widget; children are rows

    SetValCb  valCb_;
    SetFileCb fileCb_;

    // Per-control bookkeeping (heap-allocated; deleted in clear())
    struct FloatData {
        ParameterPanel* panel;
        uint32_t portIndex;
        float min, max, current;
    };
    struct ToggleData {
        ParameterPanel* panel;
        uint32_t portIndex;
        float onValue, offValue;
    };
    struct TriggerData {
        ParameterPanel* panel;
        uint32_t portIndex;
        float onValue;
    };
    struct EnumData {
        ParameterPanel* panel;
        uint32_t portIndex;
        float value;         // current enum value
        int  selectedIdx;
    };
    struct EnumItemData {
        ParameterPanel* panel;
        uint32_t portIndex;
        float    value;
        int      idx;
        Widget   menuBtn;    // pointer back to the MenuButton
    };
    struct FileData {
        ParameterPanel* panel;
        uint32_t portIndex;
    };

    std::vector<FloatData*>    floatItems_;
    std::vector<ToggleData*>   toggleItems_;
    std::vector<TriggerData*>  triggerItems_;
    std::vector<EnumData*>     enumItems_;
    std::vector<EnumItemData*> enumItemItems_;
    std::vector<FileData*>     fileItems_;
    std::vector<Widget>        rowWidgets_;
    std::vector<Widget>        menuWidgets_;  // SimpleMenu widgets to destroy
};
