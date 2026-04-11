// src/xlib/ControlBar.cpp — Xlib/Xaw port

#include "ControlBar.h"

#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <cstdio>
#include <cstring>

// ── Constructor ───────────────────────────────────────────────────────────────

ControlBar::ControlBar(Widget parent, XlibApp& app) : app_(app) {
    buildWidgets(parent);
}

// ── Widget construction ───────────────────────────────────────────────────────

void ControlBar::buildWidgets(Widget parent) {
    bar_ = XtVaCreateManagedWidget("controlBar",
        boxWidgetClass, parent,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);

    // ── Power toggle ─────────────────────────────────────────────────────
    powerToggle_ = XtVaCreateManagedWidget("Power",
        toggleWidgetClass, bar_,
        XtNlabel, "  Power  ",
        nullptr);
    XtAddCallback(powerToggle_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<ControlBar*>(client)->onPowerToggled();
        }, this);

    // ── Gain scrollbar ────────────────────────────────────────────────────
    XtVaCreateManagedWidget("gainLbl", labelWidgetClass, bar_,
        XtNlabel, " Gain:", XtNborderWidth, 0, nullptr);

    gainSlider_ = XtVaCreateManagedWidget("gain",
        scrollbarWidgetClass, bar_,
        XtNorientation, XtorientHorizontal,
        XtNwidth,       120,
        XtNheight,      20,
        XtNthickness,   10,
        nullptr);

    // jump proc (absolute position click)
    XtAddCallback(gainSlider_, XtNjumpProc,
        [](Widget, XtPointer client, XtPointer callData) {
            float* pos = reinterpret_cast<float*>(callData);
            static_cast<ControlBar*>(client)->onGainJump(*pos);
        }, this);
    // scroll proc (arrow clicks)
    XtAddCallback(gainSlider_, XtNscrollProc,
        [](Widget, XtPointer client, XtPointer callData) {
            int delta = (int)(intptr_t)callData;
            static_cast<ControlBar*>(client)->onGainScroll(delta);
        }, this);

    // Set initial thumb position
    float norm = (currentGain_ - gainMin_) / (gainMax_ - gainMin_);
    XawScrollbarSetThumb(gainSlider_, norm, 0.05f);

    // ── Format menu ───────────────────────────────────────────────────────
    XtVaCreateManagedWidget("fmtLbl", labelWidgetClass, bar_,
        XtNlabel, " Format:", XtNborderWidth, 0, nullptr);

    formatBtn_ = XtVaCreateManagedWidget("format",
        menuButtonWidgetClass, bar_,
        XtNmenuName, "formatMenu",
        XtNlabel,    "WAV",
        nullptr);
    buildFormatMenu();

    // ── Quality menu (initially hidden if format=WAV) ─────────────────────
    qualityBox_ = XtVaCreateManagedWidget("qualityBox",
        boxWidgetClass, bar_,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        nullptr);

    XtVaCreateManagedWidget("qualLbl", labelWidgetClass, qualityBox_,
        XtNlabel, " Quality:", XtNborderWidth, 0, nullptr);

    qualityBtn_ = XtVaCreateManagedWidget("quality",
        menuButtonWidgetClass, qualityBox_,
        XtNmenuName, "qualityMenu",
        XtNlabel,    "5",
        nullptr);
    buildQualityMenu();
    updateQualityVisibility();

    // ── Record toggle ─────────────────────────────────────────────────────
    recordToggle_ = XtVaCreateManagedWidget("Record",
        toggleWidgetClass, bar_,
        XtNlabel, "  Record  ",
        nullptr);
    XtAddCallback(recordToggle_, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<ControlBar*>(client)->onRecordToggled();
        }, this);

    // ── Xrun counter ──────────────────────────────────────────────────────
    xrunLabel_ = XtVaCreateManagedWidget("xrunLbl",
        labelWidgetClass, bar_,
        XtNlabel,       " Xruns: 0 ",
        XtNborderWidth, 0,
        nullptr);

    // ── Status label ──────────────────────────────────────────────────────
    statusLabel_ = XtVaCreateManagedWidget("statusLbl",
        labelWidgetClass, bar_,
        XtNlabel,       " Ready ",
        XtNborderWidth, 0,
        XtNjustify,     XtJustifyLeft,
        nullptr);
}

// ── Format menu ───────────────────────────────────────────────────────────────

void ControlBar::buildFormatMenu() {
    static const char* names[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
    Widget menu = XtVaCreatePopupShell("formatMenu",
        simpleMenuWidgetClass, formatBtn_, nullptr);
    formatMenu_ = menu;

    for (int i = 0; i < 5; ++i) {
        auto* d = new DropItem{this, i};
        formatItems_.push_back(d);
        Widget item = XtVaCreateManagedWidget(names[i],
            smeBSBObjectClass, menu, nullptr);
        XtAddCallback(item, XtNcallback,
            [](Widget, XtPointer client, XtPointer) {
                DropItem* di = static_cast<DropItem*>(client);
                di->bar->onFormatSelected(di->index);
            }, d);
    }
}

// ── Quality menu ──────────────────────────────────────────────────────────────

void ControlBar::buildQualityMenu() {
    Widget menu = XtVaCreatePopupShell("qualityMenu",
        simpleMenuWidgetClass, qualityBtn_, nullptr);
    qualityMenu_ = menu;

    for (int i = 0; i <= 9; ++i) {
        char label[4];
        snprintf(label, sizeof(label), "%d", i);
        auto* d = new DropItem{this, i};
        qualityItems_.push_back(d);
        Widget item = XtVaCreateManagedWidget(label,
            smeBSBObjectClass, menu, nullptr);
        XtAddCallback(item, XtNcallback,
            [](Widget, XtPointer client, XtPointer) {
                DropItem* di = static_cast<DropItem*>(client);
                di->bar->onQualitySelected(di->index);
            }, d);
    }
}

// ── Callbacks ─────────────────────────────────────────────────────────────────

void ControlBar::onPowerToggled() {
    if (suppressSignals_) return;
    Boolean state = False;
    XtVaGetValues(powerToggle_, XtNstate, &state, nullptr);
    powerActive_ = (state == True);
    if (powerCb_) powerCb_(powerActive_);
}

void ControlBar::onGainJump(float pos) {
    currentGain_ = gainMin_ + pos * (gainMax_ - gainMin_);
    XawScrollbarSetThumb(gainSlider_, pos, 0.05f);
    if (gainCb_) gainCb_(currentGain_);
}

void ControlBar::onGainScroll(int delta) {
    float range = gainMax_ - gainMin_;
    float step  = range * 0.02f * (delta > 0 ? 1.0f : -1.0f);
    currentGain_ += step;
    if (currentGain_ < gainMin_) currentGain_ = gainMin_;
    if (currentGain_ > gainMax_) currentGain_ = gainMax_;

    float pos = (currentGain_ - gainMin_) / range;
    XawScrollbarSetThumb(gainSlider_, pos, 0.05f);
    if (gainCb_) gainCb_(currentGain_);
}

void ControlBar::onFormatSelected(int idx) {
    currentFormat_ = idx;
    static const char* names[] = {"WAV", "MP3", "OGG", "OPUS", "FLAC"};
    XtVaSetValues(formatBtn_, XtNlabel, names[idx], nullptr);
    updateQualityVisibility();
}

void ControlBar::onQualitySelected(int idx) {
    currentQuality_ = idx;
    char label[4];
    snprintf(label, sizeof(label), "%d", idx);
    XtVaSetValues(qualityBtn_, XtNlabel, label, nullptr);
}

void ControlBar::onRecordToggled() {
    if (suppressSignals_) return;
    Boolean state = False;
    XtVaGetValues(recordToggle_, XtNstate, &state, nullptr);
    recordActive_ = (state == True);
    if (recordCb_) recordCb_(recordActive_, currentFormat_, currentQuality_);
}

// ── Quality visibility ────────────────────────────────────────────────────────

void ControlBar::updateQualityVisibility() {
    // WAV has no quality setting; for all other formats show quality
    if (currentFormat_ == 0) {
        XtUnmanageChild(qualityBox_);
    } else {
        XtManageChild(qualityBox_);
    }
}

// ── State setters ─────────────────────────────────────────────────────────────

void ControlBar::setPowerState(bool on) {
    suppressSignals_ = true;
    powerActive_ = on;
    XtVaSetValues(powerToggle_, XtNstate, on ? True : False, nullptr);
    suppressSignals_ = false;
}

void ControlBar::setRecordingActive(bool active) {
    suppressSignals_ = true;
    recordActive_ = active;
    XtVaSetValues(recordToggle_, XtNstate, active ? True : False, nullptr);
    suppressSignals_ = false;
}

void ControlBar::setGainValue(float gain) {
    currentGain_ = gain;
    float pos = (gain - gainMin_) / (gainMax_ - gainMin_);
    if (pos < 0.0f) pos = 0.0f;
    if (pos > 1.0f) pos = 1.0f;
    XawScrollbarSetThumb(gainSlider_, pos, 0.05f);
}

void ControlBar::setStatusText(const std::string& text) {
    XtVaSetValues(statusLabel_, XtNlabel, text.c_str(), nullptr);
}

void ControlBar::setXrunCount(uint64_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), " Xruns: %llu ", (unsigned long long)n);
    XtVaSetValues(xrunLabel_, XtNlabel, buf, nullptr);
}
