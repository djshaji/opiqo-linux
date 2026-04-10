// src/gtk3/ControlBar.cpp (GTK3 port)

#include "ControlBar.h"

#include <cstdio>
#include <string>

// ── ControlBar ────────────────────────────────────────────────────────────────

ControlBar::ControlBar(GtkWidget* parent_box) {
    buildWidgets();
    if (parent_box)
        // GTK3: gtk_box_pack_start instead of gtk_box_append
        gtk_box_pack_start(GTK_BOX(parent_box), bar_, FALSE, FALSE, 0);
}

void ControlBar::buildWidgets() {
    // ── Outer bar ─────────────────────────────────────────────────────────
    bar_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(bar_, 8);
    gtk_widget_set_margin_end(bar_, 8);
    gtk_widget_set_margin_top(bar_, 4);
    gtk_widget_set_margin_bottom(bar_, 4);

    // ── Power toggle ──────────────────────────────────────────────────────
    powerToggle_ = gtk_toggle_button_new_with_label("  Power  ");
    gtk_widget_set_tooltip_text(powerToggle_, "Start / stop audio engine");
    g_signal_connect_swapped(powerToggle_, "toggled",
                             G_CALLBACK(+[](ControlBar* self) { self->onPowerToggled(); }),
                             this);
    gtk_box_pack_start(GTK_BOX(bar_), powerToggle_, FALSE, FALSE, 0);

    // ── Separator ─────────────────────────────────────────────────────────
    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    // ── Gain label + slider ───────────────────────────────────────────────
    GtkWidget* gainLabel = gtk_label_new("Gain");
    gtk_box_pack_start(GTK_BOX(bar_), gainLabel, FALSE, FALSE, 0);

    gainScale_ = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 2.0, 0.01);
    gtk_range_set_value(GTK_RANGE(gainScale_), 1.0);
    gtk_scale_set_draw_value(GTK_SCALE(gainScale_), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(gainScale_), GTK_POS_RIGHT);
    gtk_widget_set_size_request(gainScale_, 300, -1);
    gtk_widget_set_tooltip_text(gainScale_, "Output gain (linear)");
    g_signal_connect_swapped(gainScale_, "value-changed",
                             G_CALLBACK(+[](ControlBar* self) { self->onGainChanged(); }),
                             this);
    gtk_box_pack_start(GTK_BOX(bar_), gainScale_, FALSE, FALSE, 0);

    // ── Separator ─────────────────────────────────────────────────────────
    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    // ── Format dropdown ───────────────────────────────────────────────────
    GtkWidget* fmtLabel = gtk_label_new("Format");
    gtk_box_pack_start(GTK_BOX(bar_), fmtLabel, FALSE, FALSE, 0);

    // GTK3: GtkComboBoxText instead of GtkDropDown + GtkStringList
    formatDrop_ = gtk_combo_box_text_new();
    // Keep order in sync with FileType enum: WAV=0, MP3=1, OGG=2, OPUS=3, FLAC=4
    const char* formats[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
    for (const char* f : formats)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(formatDrop_), f);
    gtk_combo_box_set_active(GTK_COMBO_BOX(formatDrop_), 0);
    gtk_widget_set_tooltip_text(formatDrop_, "Recording file format");
    // GTK3: "changed" signal instead of "notify::selected"
    g_signal_connect_swapped(formatDrop_, "changed",
                             G_CALLBACK(+[](ControlBar* self) { self->onFormatChanged(); }),
                             this);
    gtk_box_pack_start(GTK_BOX(bar_), formatDrop_, FALSE, FALSE, 0);

    // ── Quality dropdown (hidden for WAV / FLAC) ──────────────────────────
    qualityBox_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* qLabel = gtk_label_new("Quality");
    gtk_box_pack_start(GTK_BOX(qualityBox_), qLabel, FALSE, FALSE, 0);

    // GTK3: GtkComboBoxText
    qualityDrop_ = gtk_combo_box_text_new();
    const char* qualities[] = {"0 (low)", "1", "2", "3", "4", "5 (med)",
                                "6", "7", "8", "9 (high)"};
    for (const char* q : qualities)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(qualityDrop_), q);
    gtk_combo_box_set_active(GTK_COMBO_BOX(qualityDrop_), 5);
    gtk_box_pack_start(GTK_BOX(qualityBox_), qualityDrop_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bar_), qualityBox_, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(qualityBox_, TRUE);  // GTK3: hidden by default
    gtk_widget_hide(qualityBox_);

    // ── Separator ─────────────────────────────────────────────────────────
    gtk_box_pack_start(GTK_BOX(bar_),
        gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    // ── Record toggle ─────────────────────────────────────────────────────
    recordToggle_ = gtk_toggle_button_new_with_label("⏺  Record");
    gtk_widget_set_tooltip_text(recordToggle_, "Start / stop recording");
    g_signal_connect_swapped(recordToggle_, "toggled",
                             G_CALLBACK(+[](ControlBar* self) { self->onRecordToggled(); }),
                             this);
    gtk_box_pack_start(GTK_BOX(bar_), recordToggle_, FALSE, FALSE, 0);

    // ── Spacer (push xrun + status to the right) ──────────────────────────
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(bar_), spacer, TRUE, TRUE, 0);

    // ── Xrun counter ──────────────────────────────────────────────────────
    xrunLabel_ = gtk_label_new("Xruns: 0");
    // GTK3: gtk_style_context_add_class instead of gtk_widget_add_css_class
    gtk_style_context_add_class(
        gtk_widget_get_style_context(xrunLabel_), "xrun-label");
    gtk_widget_set_tooltip_text(xrunLabel_, "JACK buffer underruns");
    gtk_box_pack_end(GTK_BOX(bar_), xrunLabel_, FALSE, FALSE, 0);

    // ── Status label ──────────────────────────────────────────────────────
    statusLabel_ = gtk_label_new("Ready");
    gtk_widget_set_hexpand(statusLabel_, FALSE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(statusLabel_), "status-label");
    gtk_box_pack_end(GTK_BOX(bar_), statusLabel_, FALSE, FALSE, 0);
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void ControlBar::onPowerToggled() {
    if (suppressSignals_) return;
    if (powerCb_)
        powerCb_(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(powerToggle_)));
}

void ControlBar::onGainChanged() {
    if (suppressSignals_) return;
    if (gainCb_)
        gainCb_(static_cast<float>(gtk_range_get_value(GTK_RANGE(gainScale_))));
}

void ControlBar::onFormatChanged() {
    // GTK3: gtk_combo_box_get_active instead of gtk_drop_down_get_selected
    const int fmt = gtk_combo_box_get_active(GTK_COMBO_BOX(formatDrop_));
    const bool lossy = (fmt == 1 || fmt == 2 || fmt == 3);
    gtk_widget_set_visible(qualityBox_, lossy ? TRUE : FALSE);
}

void ControlBar::onRecordToggled() {
    if (suppressSignals_) return;
    const bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(recordToggle_));
    const int fmt  = gtk_combo_box_get_active(GTK_COMBO_BOX(formatDrop_));
    const int qual = gtk_combo_box_get_active(GTK_COMBO_BOX(qualityDrop_));
    if (recordCb_)
        recordCb_(active, fmt, qual);
}

// ── State setters ─────────────────────────────────────────────────────────────

void ControlBar::setPowerState(bool on) {
    suppressSignals_ = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(powerToggle_), on ? TRUE : FALSE);
    suppressSignals_ = false;
}

void ControlBar::setRecordingActive(bool active) {
    suppressSignals_ = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recordToggle_), active ? TRUE : FALSE);
    gtk_button_set_label(GTK_BUTTON(recordToggle_), active ? "⏹  Stop Rec" : "⏺  Record");
    suppressSignals_ = false;
}

void ControlBar::setStatusText(const std::string& text) {
    gtk_label_set_text(GTK_LABEL(statusLabel_), text.c_str());
}

void ControlBar::setXrunCount(uint64_t n) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Xruns: %llu", (unsigned long long)n);
    gtk_label_set_text(GTK_LABEL(xrunLabel_), buf);
}
