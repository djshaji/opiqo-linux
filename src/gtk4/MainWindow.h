// src/gtk4/MainWindow.h
// Top-level application window.
// Owns: LiveEffectEngine, AudioEngine, JackPortEnum, AppSettings,
//       ControlBar, 4 × PluginSlot, SettingsDialog.

#pragma once

#include <array>
#include <memory>
#include <string>

#include <gtk/gtk.h>
#include <sys/stat.h>

#include "AppSettings.h"
#include "AudioEngine.h"
#include "ControlBar.h"
#include "JackPortEnum.h"
#include "LiveEffectEngine.h"
#include "PluginSlot.h"
#include "SettingsDialog.h"
#include "version.h"

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow();

    GtkWidget* window() const { return window_; }
    void savePluginCache () const;  // for debugging: saves plugin info JSON to ~/opiqo_plugin_cache.json
    bool loadPluginCache () const;  // for debugging: loads plugin info JSON from ~/opiqo_plugin_cache.json (if present) and prints to log
    void testPluginLoadUnload ();
private:
    // ── Widget construction ───────────────────────────────────────────────
    void buildWidgets();
    void loadCss();

    // ── Engine lifecycle ──────────────────────────────────────────────────
    void onPowerToggled(bool on);
    void onGainChanged(float gain);

    // ── Recording ─────────────────────────────────────────────────────────
    void onRecordToggled(bool start, int format, int quality);

    // ── Plugin management ─────────────────────────────────────────────────
    void onAddPlugin(int slot);
    void onDeletePlugin(int slot);
    void onBypassPlugin(int slot, bool bypassed);
    void onSetValue(int slot, uint32_t portIndex, float value);
    void onSetFilePath(int slot, const std::string& uri, const std::string& path);

    // ── Settings ──────────────────────────────────────────────────────────
    void openSettings();
    void onSettingsApply(const AppSettings& s);
    std::string onExportPreset();
    void onImportPreset(const std::string& path);

    // ── Periodic poll (200 ms timer on main thread) ───────────────────────
    static gboolean pollEngineState(gpointer data);

    void setStatus(const std::string& msg);
    void showAboutDialog();

    // ── Widgets ───────────────────────────────────────────────────────────
    GtkWidget* window_     = nullptr;
    GtkWidget* slotGrid_   = nullptr;

    std::unique_ptr<ControlBar>    controlBar_;
    std::array<PluginSlot*, 4>     slots_     = {};  // indices 0–3 (slot index 1–4)
    std::unique_ptr<SettingsDialog> settingsDlg_;

    // ── Domain objects ────────────────────────────────────────────────────
    std::unique_ptr<LiveEffectEngine> engine_;
    std::unique_ptr<AudioEngine>      audio_;
    std::unique_ptr<JackPortEnum>     portEnum_;
    AppSettings                        settings_;

    bool   isRecording_    = false;
    guint  pollTimerId_    = 0;
};
