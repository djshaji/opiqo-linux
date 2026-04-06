// src/linux/AppSettings.h
// XDG-compliant user preference storage for Opiqo.
// All fields carry safe defaults so the app starts cleanly without any saved config.

#pragma once

#include <string>

struct AppSettings {
    // ── Audio ────────────────────────────────────────────────────────────────
    std::string capturePort   = "";   // e.g. "system:capture_1"
    std::string capturePort2  = "";   // e.g. "system:capture_2"
    std::string playbackPort  = "";   // e.g. "system:playback_1"
    std::string playbackPort2 = "";   // e.g. "system:playback_2"

    // ── Recording  ───────────────────────────────────────────────────────────
    // 0=WAV 1=MP3 2=OGG 3=OPUS 4=FLAC  (matches FileType enum in FileWriter.h)
    int   recordFormat  = 0;
    // 0=lowest … 9=highest quality (meaning varies per codec)
    int   recordQuality = 5;

    // ── Engine ───────────────────────────────────────────────────────────────
    float gain          = 1.0f;   // linear gain applied after the plugin chain

    // ── Persistence ──────────────────────────────────────────────────────────

    // Load from $XDG_CONFIG_HOME/opiqo/settings.json
    // (falls back to ~/.config/opiqo/settings.json).
    // Returns default-constructed AppSettings on any error.
    static AppSettings load();

    // Save to the same path as load().
    void save() const;

private:
    static std::string configPath();
};
