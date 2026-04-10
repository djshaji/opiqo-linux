// src/gtk3/ControlBar.h (GTK3 port)
// Bottom control bar: Power toggle, Gain slider, Record toggle, Format/Quality dropdowns.

#pragma once

#include <functional>
#include <string>
#include <gtk/gtk.h>

class ControlBar {
public:
    using PowerCb    = std::function<void(bool on)>;
    using GainCb     = std::function<void(float gain)>;
    using RecordCb   = std::function<void(bool start, int format, int quality)>;

    explicit ControlBar(GtkWidget* parent_box);
    ~ControlBar() = default;

    GtkWidget* widget() const { return bar_; }

    void setPowerCallback(PowerCb  cb) { powerCb_  = std::move(cb); }
    void setGainCallback (GainCb   cb) { gainCb_   = std::move(cb); }
    void setRecordCallback(RecordCb cb){ recordCb_ = std::move(cb); }

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
    GtkWidget* formatDrop_    = nullptr;  // GtkComboBoxText in GTK3
    GtkWidget* qualityDrop_   = nullptr;  // GtkComboBoxText in GTK3
    GtkWidget* qualityBox_    = nullptr;
    GtkWidget* statusLabel_   = nullptr;
    GtkWidget* xrunLabel_     = nullptr;

    PowerCb  powerCb_;
    GainCb   gainCb_;
    RecordCb recordCb_;

    bool suppressSignals_ = false;
};
