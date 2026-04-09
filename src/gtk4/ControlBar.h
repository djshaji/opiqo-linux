// src/gtk4/ControlBar.h
// Bottom control bar: Power toggle, Gain slider, Record toggle, Format/Quality dropdowns.
// All callbacks run on the GTK main thread.

#pragma once

#include <functional>
#include <string>
#include <gtk/gtk.h>

class ControlBar {
public:
    // Callback signatures
    using PowerCb    = std::function<void(bool on)>;
    using GainCb     = std::function<void(float gain)>;
    using RecordCb   = std::function<void(bool start, int format, int quality)>;

    explicit ControlBar(GtkWidget* parent_box);
    ~ControlBar() = default;

    GtkWidget* widget() const { return bar_; }

    // Wire application-level actions
    void setPowerCallback(PowerCb  cb) { powerCb_  = std::move(cb); }
    void setGainCallback (GainCb   cb) { gainCb_   = std::move(cb); }
    void setRecordCallback(RecordCb cb){ recordCb_ = std::move(cb); }

    // Reflect engine state back to the UI
    void setPowerState(bool on);
    void setRecordingActive(bool active);
    void setStatusText(const std::string& text);
    void setXrunCount(uint64_t n);

private:
    void buildWidgets();
    void onPowerToggled();
    void onGainChanged();
    void onRecordToggled();
    void onFormatChanged();

    GtkWidget* bar_           = nullptr;
    GtkWidget* powerToggle_   = nullptr;
    GtkWidget* gainScale_     = nullptr;
    GtkWidget* recordToggle_  = nullptr;
    GtkWidget* formatDrop_    = nullptr;
    GtkWidget* qualityDrop_   = nullptr;
    GtkWidget* qualityBox_    = nullptr;   // hidden for lossless formats
    GtkWidget* statusLabel_   = nullptr;
    GtkWidget* xrunLabel_     = nullptr;

    PowerCb  powerCb_;
    GainCb   gainCb_;
    RecordCb recordCb_;

    bool suppressSignals_ = false;
};
