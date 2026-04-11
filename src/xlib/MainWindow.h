// src/xlib/MainWindow.h — Xlib/Xaw port
// Top-level application window.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <functional>
#include <string>
#include <memory>
#include <cstdint>
#include "XlibApp.h"
#include "AppSettings.h"
#include "AudioEngine.h"
#include "JackPortEnum.h"
#include "LiveEffectEngine.h"

class ControlBar;
class PresetBar;
class PluginSlot;
class SettingsDialog;

class MainWindow {
public:
    explicit MainWindow(XlibApp& app);
    ~MainWindow();

private:
    void buildWidgets();
    void connectCallbacks();
    void startEngineTimer();

    // ── Power / settings callbacks ─────────────────────────────────────
    void onPowerToggled(bool on);
    void onSettingsOpen();
    void onSettingsApply(const AppSettings& s);

    // ── Audio engine helpers ───────────────────────────────────────────
    void onGainChanged(float gain);
    void onRecordToggled(bool start, int format, int quality);

    // ── Plugin slot callbacks ──────────────────────────────────────────
    void onAddPlugin   (int slot);
    void onDeletePlugin(int slot);
    void onBypassPlugin(int slot, bool bypassed);
    void onSetValue    (int slot, uint32_t portIdx, float value);
    void onSetFilePath (int slot, uint32_t portIdx, const std::string& path);

    // ── Preset management ─────────────────────────────────────────────
    void onPresetLoad();
    void onPresetSave  (const std::string& name);
    void onPresetDelete();
    void onExportPreset();
    void onImportPreset(const std::string& path);
    void applyFullPreset(const nlohmann::json& j);
    void loadNamedPresets();
    void saveNamedPresets();
    std::string namedPresetsPath() const;
    bool loadPluginCache();
    void savePluginCache();

    // ── Engine state poll ─────────────────────────────────────────────
    static void enginePollCB(XtPointer client, XtIntervalId* id);
    void        pollEngineState();

    // ── Helpers ───────────────────────────────────────────────────────
    void setStatus(const std::string& msg);
    std::string askForFilePath(const std::string& title, bool forSave);

    // ── Data ──────────────────────────────────────────────────────────
    XlibApp& app_;

    Widget   mainForm_    = nullptr;  // VPaned
    Widget   menuBar_     = nullptr;
    Widget   slotGrid_    = nullptr;  // Form for the 2×2 plugin grid
    Widget   settingsBtn_ = nullptr;
    Widget   aboutBtn_    = nullptr;

    std::unique_ptr<ControlBar>    controlBar_;
    std::unique_ptr<PresetBar>     presetBar_;
    std::unique_ptr<PluginSlot>    slots_[4];
    std::unique_ptr<SettingsDialog> settingsDlg_;

    AppSettings                      settings_;
    std::unique_ptr<JackPortEnum>    portEnum_;
    std::unique_ptr<AudioEngine>     audioEngine_;
    std::unique_ptr<LiveEffectEngine>engine_;

    std::string pluginsJson_;
    nlohmann::json namedPresets_;

    XtIntervalId pollTimerId_ = 0;
};
