// src/xlib/ControlBar.h — Xlib/Xaw port
// Bottom control bar: Power toggle, Gain scrollbar, Format/Quality dropdowns,
// Record toggle, xrun counter, status label.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <cstdint>
#include <vector>
#include "XlibApp.h"

class ControlBar {
public:
    using PowerCb   = std::function<void(bool on)>;
    using GainCb    = std::function<void(float gain)>;
    using RecordCb  = std::function<void(bool start, int format, int quality)>;

    ControlBar(Widget parent, XlibApp& app);
    ~ControlBar() = default;

    Widget widget() const { return bar_; }

    void setPowerCallback (PowerCb  cb) { powerCb_  = std::move(cb); }
    void setGainCallback  (GainCb   cb) { gainCb_   = std::move(cb); }
    void setRecordCallback(RecordCb cb) { recordCb_ = std::move(cb); }

    void setPowerState      (bool on);
    void setRecordingActive (bool active);
    void setGainValue       (float gain);
    void setStatusText      (const std::string& text);
    void setXrunCount       (uint64_t n);

    // Callbacks invoked from Xt callbacks
    void onPowerToggled();
    void onGainJump(float pos);
    void onGainScroll(int delta);
    void onFormatSelected(int idx);
    void onQualitySelected(int idx);
    void onRecordToggled();

private:
    void buildWidgets(Widget parent);
    void buildFormatMenu();
    void buildQualityMenu();
    void updateQualityVisibility();

    XlibApp&  app_;
    Widget    bar_          = nullptr;
    Widget    powerToggle_  = nullptr;
    Widget    gainSlider_   = nullptr;
    Widget    formatBtn_    = nullptr;
    Widget    formatMenu_   = nullptr;
    Widget    qualityBtn_   = nullptr;
    Widget    qualityMenu_  = nullptr;
    Widget    qualityBox_   = nullptr;  // Box containing quality label+btn
    Widget    recordToggle_ = nullptr;
    Widget    xrunLabel_    = nullptr;
    Widget    statusLabel_  = nullptr;

    int   currentFormat_  = 0;    // 0=WAV 1=MP3 2=OGG 3=OPUS 4=FLAC
    int   currentQuality_ = 5;
    float gainMin_        = 0.0f;
    float gainMax_        = 2.0f;
    float currentGain_    = 1.0f;

    bool  suppressSignals_ = false;
    bool  powerActive_     = false;
    bool  recordActive_    = false;

    // Per-item callback data (stored so we can delete on rebuild)
    struct DropItem { ControlBar* bar; int index; };
    std::vector<DropItem*> formatItems_;
    std::vector<DropItem*> qualityItems_;

    PowerCb  powerCb_;
    GainCb   gainCb_;
    RecordCb recordCb_;
};
