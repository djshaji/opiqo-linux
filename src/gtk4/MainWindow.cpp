// src/gtk4/MainWindow.cpp

#include "MainWindow.h"

#include "PluginDialog.h"
#include "PresetBar.h"
#include "logging_macros.h"
#include "json.hpp"

#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

using json = nlohmann::json;

// ── Embedded CSS ──────────────────────────────────────────────────────────────

static const char* kAppCss = R"CSS(
window {
}
.plugin-slot {
}
.slot-header {
}
.slot-name {
}
.status-label {
}
.xrun-label {
}
.plugin-name {
}
)CSS";

// ── MainWindow ────────────────────────────────────────────────────────────────

MainWindow::MainWindow(GtkApplication* app) {
    // ── Domain objects ────────────────────────────────────────────────────
    engine_   = std::make_unique<LiveEffectEngine>();
    settings_ = AppSettings::load();
    portEnum_ = std::make_unique<JackPortEnum>();
    audio_    = std::make_unique<AudioEngine>(engine_.get());

    // Scan installed LV2 plugins (relatively slow — do once at startup)
    if (! loadPluginCache()) {
        LOGD("No plugin cache loaded; scanning plugins...");
        engine_->initPlugins();
        savePluginCache();
    } else {
        LOGD("Plugin cache loaded successfully");
    }

    // Wire JACK hot-plug change notification to re-open settings dialog hint
    portEnum_->setChangeCallback([this]() {
        setStatus("JACK ports changed — check Settings to update routing");
    });

    // Wire audio engine error feedback
    audio_->setErrorCallback([this](std::string msg) {
        setStatus("Audio error: " + msg);
        controlBar_->setPowerState(false);
    });

    // ── GTK window ────────────────────────────────────────────────────────
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "Opiqo");
    gtk_window_set_default_size(GTK_WINDOW(window_), 960, 680);

    loadCss();
    buildWidgets();

    // Load named presets from disk (non-critical; ignore failures)
    loadNamedPresets();

    // ── Periodic engine-state poll (200 ms) ───────────────────────────────
    pollTimerId_ = g_timeout_add(200, pollEngineState, this);
}

MainWindow::~MainWindow() {
    if (pollTimerId_) g_source_remove(pollTimerId_);

    if (isRecording_) engine_->stopRecording();
    audio_->stop();
    settings_.save();

    for (auto* s : slots_) delete s;
}

// ── CSS ───────────────────────────────────────────────────────────────────────

void MainWindow::loadCss() {
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, kAppCss);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

// ── Widget construction ───────────────────────────────────────────────────────

void MainWindow::buildWidgets() {
    // ── Header bar ────────────────────────────────────────────────────────
    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    // GtkWidget* settingsBtn = gtk_button_new_from_icon_name("preferences-system-symbolic");
    GtkWidget* settingsBtn = gtk_button_new_with_label("Settings");
    gtk_widget_set_tooltip_text(settingsBtn, "Settings");
    g_signal_connect_swapped(settingsBtn, "clicked",
        G_CALLBACK(+[](MainWindow* self) { self->openSettings(); }), this);

    GtkWidget * debugBtn = gtk_button_new_with_label("Test");
    gtk_widget_set_tooltip_text(debugBtn, "Test plugin load/unload");
    g_signal_connect_swapped(debugBtn, "clicked",
        G_CALLBACK(+[](MainWindow* self) { self->testPluginLoadUnload(); }), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), debugBtn);
    gtk_widget_set_visible(debugBtn, false);  // hide for now; only used for development testing

    GtkWidget* title = gtk_label_new("Opiqo");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), title);

    GtkWidget* aboutBtn = gtk_button_new_with_label("About");
    gtk_widget_set_tooltip_text(aboutBtn, "About Opiqo");
    g_signal_connect_swapped(aboutBtn, "clicked",
        G_CALLBACK(+[](MainWindow* self) { self->showAboutDialog(); }), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), aboutBtn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), settingsBtn);

    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    // ── Root vertical box ─────────────────────────────────────────────────
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window_), root);

    // ── 2×2 plugin slot grid ──────────────────────────────────────────────
    slotGrid_ = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(slotGrid_), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(slotGrid_), TRUE);
    gtk_widget_set_vexpand(slotGrid_, TRUE);
    gtk_widget_set_hexpand(slotGrid_, TRUE);
    gtk_box_append(GTK_BOX(root), slotGrid_);

    // Create slots 1–4 (stored at indices 0–3)
    const int positions[4][2] = {{0,0},{1,0},{0,1},{1,1}};
    for (int i = 0; i < 4; ++i) {
        auto* slot = new PluginSlot(i + 1, window_);

        slot->setAddCallback   ([this](int s)         { onAddPlugin(s); });
        slot->setDeleteCallback([this](int s)         { onDeletePlugin(s); });
        slot->setBypassCallback([this](int s, bool b) { onBypassPlugin(s, b); });
        slot->setValueCallback ([this](int s, uint32_t p, float v) {
            onSetValue(s, p, v);
        });
        slot->setFileCallback  ([this](int s, const std::string& u,
                                        const std::string& p) {
            onSetFilePath(s, u, p);
        });

        gtk_grid_attach(GTK_GRID(slotGrid_), slot->widget(),
                        positions[i][0], positions[i][1], 1, 1);
        // set 10 px padding around each slot
        gtk_widget_set_margin_start(slot->widget(), 10);
        gtk_widget_set_margin_end(slot->widget(), 10);
        gtk_widget_set_margin_top(slot->widget(), 10);
        gtk_widget_set_margin_bottom(slot->widget(), 10);
        slots_[i] = slot;
    }

    // ── Separator ─────────────────────────────────────────────────────────
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    // ── Preset bar ────────────────────────────────────────────────────
    presetBar_ = std::make_unique<PresetBar>(root);
    presetBar_->setLoadCallback  ([this]()                    { onPresetLoad(); });
    presetBar_->setSaveCallback  ([this](const std::string& n) { onPresetSave(n); });
    presetBar_->setDeleteCallback([this]()                    { onPresetDelete(); });
    // ── Control bar ───────────────────────────────────────────────────────
    controlBar_ = std::make_unique<ControlBar>(root);
    controlBar_->setPowerCallback ([this](bool on)           { onPowerToggled(on); });
    controlBar_->setGainCallback  ([this](float g)           { onGainChanged(g); });
    controlBar_->setRecordCallback([this](bool s, int f, int q) {
        onRecordToggled(s, f, q);
    });
}

// ── Engine lifecycle ──────────────────────────────────────────────────────────

void MainWindow::onPowerToggled(bool on) {
    if (on) {
        auto caps  = portEnum_->enumerateCapturePorts();
        auto plays = portEnum_->enumeratePlaybackPorts();

        const std::string cap1 = JackPortEnum::resolveOrDefault(caps,  settings_.capturePort);
        const std::string cap2 = JackPortEnum::resolveOrDefault(caps,  settings_.capturePort2);
        const std::string pb1  = JackPortEnum::resolveOrDefault(plays, settings_.playbackPort);
        const std::string pb2  = JackPortEnum::resolveOrDefault(plays, settings_.playbackPort2);

        if (!audio_->start(cap1, cap2, pb1, pb2)) {
            // Error already surfaced via error callback; revert toggle
            controlBar_->setPowerState(false);
            return;
        }
        setStatus("Engine running  •  " +
                  std::to_string(audio_->sampleRate()) + " Hz  •  " +
                  std::to_string(audio_->blockSize())  + " frames/block");

        // Update settings dialog info if it was already created
        if (settingsDlg_)
            settingsDlg_->updateAudioInfo(audio_->sampleRate(), audio_->blockSize());
    } else {
        if (isRecording_) {
            engine_->stopRecording();
            controlBar_->setRecordingActive(false);
            isRecording_ = false;
        }
        audio_->stop();
        setStatus("Engine stopped");
    }
}

void MainWindow::onGainChanged(float gain) {
    if (engine_->gain)
        *engine_->gain = gain;
    settings_.gain = gain;
}

// ── Recording ─────────────────────────────────────────────────────────────────

void MainWindow::onRecordToggled(bool start, int format, int quality) {
    if (start) {
        if (audio_->state() != AudioEngine::State::Running) {
            setStatus("Start the engine before recording");
            controlBar_->setRecordingActive(false);
            return;
        }

        // Generate timestamped output path
        const char* musicDir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);
        if (!musicDir) musicDir = g_get_home_dir();

        static const char* exts[] = {".wav", ".mp3", ".ogg", ".opus", ".flac"};
        const char* ext = (format >= 0 && format < 5) ? exts[format] : ".wav";

        char filename[128];
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        strftime(filename, sizeof(filename), "opiqo-%Y%m%d-%H%M%S", t);

        std::string path = std::string(musicDir) + "/" + filename + ext;

        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            setStatus("Cannot create recording file: " + path);
            controlBar_->setRecordingActive(false);
            return;
        }

        if (!engine_->startRecording(fd, format, quality)) {
            close(fd);
            setStatus("Failed to start recording");
            controlBar_->setRecordingActive(false);
            return;
        }
        isRecording_ = true;
        setStatus("Recording → " + path);
    } else {
        engine_->stopRecording();
        isRecording_ = false;
        setStatus("Recording stopped");
    }
}

// ── Plugin management ─────────────────────────────────────────────────────────

void MainWindow::onAddPlugin(int slot) {
    if (audio_->state() != AudioEngine::State::Running) {
        setStatus("Start the engine before loading plugins");
        GtkWidget* alert = gtk_message_dialog_new(GTK_WINDOW(window_),
            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            "Start the engine before loading plugins");
        g_signal_connect(alert, "response", G_CALLBACK(gtk_window_destroy), nullptr);
        gtk_widget_show(alert);
        return;
    }

    const json plugins = engine_->getAvailablePlugins();
    if (plugins.empty()) {
        setStatus("No LV2 plugins found. Install plugins and restart.");
        return;
    }

    // Show modal plugin browser
    auto* dlg = new PluginDialog(GTK_WINDOW(window_), plugins);
    dlg->show([this, slot, dlg](const std::string& uri) {
        const int rc = engine_->addPlugin(slot, uri);
        if (rc != 0) {
            setStatus("Failed to load plugin: " + uri);
        } else {
            // Determine display name
            const json all = engine_->getAvailablePlugins();
            std::string name = uri;
            if (all.contains(uri) && all[uri].contains("name"))
                name = all[uri]["name"].get<std::string>();

            const auto ports = engine_->getPluginPortInfo(slot);
            slots_[slot - 1]->onPluginAdded(name, ports);
            setStatus("Loaded: " + name);
        }
        delete dlg;
    });
}

void MainWindow::onDeletePlugin(int slot) {
    engine_->deletePlugin(slot);
    slots_[slot - 1]->onPluginCleared();
    setStatus("Plugin removed from slot " + std::to_string(slot));
}

void MainWindow::onBypassPlugin(int slot, bool bypassed) {
    engine_->setPluginEnabled(slot, !bypassed);
}

void MainWindow::onSetValue(int slot, uint32_t portIndex, float value) {
    engine_->setValue(slot, static_cast<int>(portIndex), value);
}

void MainWindow::onSetFilePath(int slot, const std::string& uri,
                                const std::string& path) {
    engine_->setFilePath(slot, uri, path);
}

// ── Settings ──────────────────────────────────────────────────────────────────

void MainWindow::openSettings() {
    if (!settingsDlg_) {
        const auto caps  = portEnum_->enumerateCapturePorts();
        const auto plays = portEnum_->enumeratePlaybackPorts();

        settingsDlg_ = std::make_unique<SettingsDialog>(GTK_WINDOW(window_),
                                                         settings_, caps, plays);
        settingsDlg_->setApplyCallback ([this](const AppSettings& s) {
            onSettingsApply(s); });
        settingsDlg_->setExportCallback([this]() { return onExportPreset(); });
        settingsDlg_->setImportCallback([this](const std::string& p) {
            onImportPreset(p); });

        if (audio_->state() == AudioEngine::State::Running)
            settingsDlg_->updateAudioInfo(audio_->sampleRate(), audio_->blockSize());
    }
    settingsDlg_->show();
}

void MainWindow::onSettingsApply(const AppSettings& s) {
    settings_ = s;
    const bool wasRunning = (audio_->state() == AudioEngine::State::Running);
    if (wasRunning) {
        audio_->stop();
        if (isRecording_) { engine_->stopRecording(); isRecording_ = false; }
        onPowerToggled(true);   // restart with new ports
    }
    settings_.save();
}

std::string MainWindow::onExportPreset() {
    return engine_->getPresetList();
}

void MainWindow::onImportPreset(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) { setStatus("Cannot open preset file"); return; }

    try {
        json j;
        f >> j;
        // Preset list is an array of slot objects
        if (j.is_array()) {
            for (int slot = 1; slot <= 4 && (slot - 1) < (int)j.size(); ++slot) {
                engine_->applyPreset(slot, j[slot - 1]);
                // Refresh parameter panel
                if (engine_->getPluginPortInfo(slot).empty()) {
                    slots_[slot - 1]->onPluginCleared();
                } else {
                    const json all = engine_->getAvailablePlugins();
                    const json& sp = j[slot - 1];
                    std::string name = "Plugin (slot " + std::to_string(slot) + ")";
                    if (sp.contains("uri") && all.contains(sp["uri"].get<std::string>())) {
                        name = all[sp["uri"].get<std::string>()]["name"].get<std::string>();
                    }
                    slots_[slot - 1]->onPluginAdded(name, engine_->getPluginPortInfo(slot));
                }
            }
        }
        setStatus("Preset imported");
    } catch (...) {
        setStatus("Failed to parse preset file");
    }
}

// ── Periodic state poll ───────────────────────────────────────────────────────

/*static*/
gboolean MainWindow::pollEngineState(gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    if (!self->audio_) return G_SOURCE_CONTINUE;

    const auto state = self->audio_->state();

    if (state == AudioEngine::State::Error) {
        self->controlBar_->setPowerState(false);
        if (self->isRecording_) {
            self->engine_->stopRecording();
            self->controlBar_->setRecordingActive(false);
            self->isRecording_ = false;
        }
    }

    self->controlBar_->setXrunCount(self->audio_->xrunCount());

    return G_SOURCE_CONTINUE;
}

// ── Status bar ────────────────────────────────────────────────────────────────

void MainWindow::showAboutDialog() {
    GtkWidget* dlg = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dlg), "Opiqo");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dlg), OPIQO_VERSION);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dlg),
        "\"" CODENAME "\"\n" MOTTO);
    const char* authors[] = { AUTHOR, nullptr };
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dlg), authors);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dlg),
        "https://github.com/djshaji/opiqo-linux");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dlg), "Source code");
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(window_));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_widget_show(dlg);
}

void MainWindow::setStatus(const std::string& msg) {
    LOGD("[status] %s", msg.c_str());
    if (controlBar_) controlBar_->setStatusText(msg);
}

void MainWindow::savePluginCache() const {
    const json cache = engine_->getAvailablePlugins();
    const std::string path = std::string (dirname ((char *) AppSettings::configPath().c_str())) + "/opiqo_plugin_cache.json";
    const char* dir = path.c_str();

    std::string dirPath = std::string(dirname((char*)AppSettings::configPath().c_str()));
    if (mkdir(dirPath.c_str(), 0755) == -1 && errno != EEXIST) {
        LOGD("Failed to create cache directory: %s", dirPath.c_str());
        return;
    }
    
    std::ofstream f(path);
    if (f.is_open()) {
        f << cache.dump(4);
        LOGD("Plugin cache saved to %s", path.c_str());
    } else {
        LOGD("Failed to save plugin cache: %s", path.c_str());
    }
}

bool MainWindow::loadPluginCache() const {
    const std::string path = std::string (dirname ((char *)AppSettings::configPath().c_str())) + "/opiqo_plugin_cache.json";
    std::ifstream f(path);
    if (!f.is_open()) {
        LOGD("Plugin cache file not found: %s", path.c_str());
        return false;
    }
    try {
        json cache;
        f >> cache;
        LOGD("Plugin cache loaded from %s", path.c_str());
        engine_->pluginInfo = cache;  // for inspection via settings dialog
        lilv_world_load_all(engine_->world);
        engine_->plugins = lilv_world_get_all_plugins(engine_->world);

        return true;
    } catch (...) {
        LOGD("Failed to parse plugin cache file: %s", path.c_str());
        return false;
    }
}

void MainWindow::testPluginLoadUnload() {
    // For debugging: repeatedly load and unload a plugin to check for memory leaks or crashes
    const json plugins = engine_->getAvailablePlugins();
    if (plugins.empty()) {
        LOGD("No plugins available for load/unload test");
        return;
    }

    for (auto it = plugins.begin(); it != plugins.end(); ++it) {
        const std::string& uri = it.key();
        int slot = 1;
        LOGD("Testing plugin load: %s", uri.c_str());
        int rc = engine_->addPlugin(slot, uri);
        if (rc != 0) {
            LOGD("Failed to load plugin: %s", uri.c_str());
            return;
        } else {
            const json all = engine_->getAvailablePlugins();
            std::string name = uri;
            if (all.contains(uri) && all[uri].contains("name"))
                name = all[uri]["name"].get<std::string>();

            const auto ports = engine_->getPluginPortInfo(slot);
            slots_[slot - 1]->onPluginAdded(name, ports);
            LOGD("Plugin loaded successfully");
        }
        engine_->deletePlugin(slot);
        LOGD("Plugin unloaded successfully");
    }

    LOGD("Plugin load/unload test completed successfully");
}

// ── Named preset management ────────────────────────────────────────────────────

std::string MainWindow::namedPresetsPath() const {
    return std::string(dirname((char*)AppSettings::configPath().c_str()))
           + "/opiqo_named_presets.json";
}

void MainWindow::loadNamedPresets() {
    const std::string path = namedPresetsPath();
    std::ifstream f(path);
    if (!f.is_open()) {
        LOGD("Named presets file not found: %s", path.c_str());
        return;
    }
    try {
        json j;
        f >> j;
        if (j.is_array()) {
            namedPresets_ = j.get<std::vector<json>>();
        }
        std::vector<std::string> names;
        for (const auto& p : namedPresets_) {
            if (p.contains("name") && p["name"].is_string())
                names.push_back(p["name"].get<std::string>());
        }
        if (presetBar_) presetBar_->setPresetNames(names);
        LOGD("Loaded %zu named presets", namedPresets_.size());
    } catch (...) {
        LOGD("Failed to parse named presets file: %s", path.c_str());
    }
}

void MainWindow::saveNamedPresets() const {
    const std::string path = namedPresetsPath();
    std::string dirPath = std::string(dirname((char*)AppSettings::configPath().c_str()));
    if (mkdir(dirPath.c_str(), 0755) == -1 && errno != EEXIST) {
        LOGD("Failed to create config directory for named presets");
        return;
    }
    json j = namedPresets_;
    std::ofstream f(path);
    if (f.is_open()) {
        f << j.dump(2) << '\n';
        LOGD("Named presets saved to %s", path.c_str());
    } else {
        LOGD("Failed to save named presets: %s", path.c_str());
    }
}

void MainWindow::applyFullPreset(const json& presetData) {
    // Apply gain
    if (presetData.contains("gain") && presetData["gain"].is_number()) {
        const float g = presetData["gain"].get<float>();
        if (engine_->gain) *engine_->gain = g;
        settings_.gain = g;
        if (controlBar_) controlBar_->setGainValue(g);
    }

    // Apply each plugin slot
    const json all = engine_->getAvailablePlugins();
    for (int slot = 1; slot <= 4; ++slot) {
        const std::string key = "plugin" + std::to_string(slot);
        if (!presetData.contains(key)) continue;
        const json& sp = presetData[key];

        // Clear the slot first
        engine_->deletePlugin(slot);
        slots_[slot - 1]->onPluginCleared();

        // Load plugin if URI present and recognised
        if (sp.is_object() && sp.contains("uri") && sp["uri"].is_string()) {
            const std::string uri = sp["uri"].get<std::string>();
            if (!uri.empty() && engine_->addPlugin(slot, uri) == 0) {
                engine_->applyPreset(slot, sp);
                std::string name = uri;
                if (all.contains(uri) && all[uri].contains("name"))
                    name = all[uri]["name"].get<std::string>();
                slots_[slot - 1]->onPluginAdded(name, engine_->getPluginPortInfo(slot));
            }
        }
    }
}

void MainWindow::onPresetLoad() {
    if (!presetBar_) return;
    const int idx = presetBar_->getSelectedIndex();
    if (idx < 0 || idx >= static_cast<int>(namedPresets_.size())) {
        setStatus("No preset selected");
        return;
    }
    const json& entry = namedPresets_[static_cast<size_t>(idx)];
    if (!entry.contains("data")) { setStatus("Invalid preset data"); return; }
    applyFullPreset(entry["data"]);
    const std::string name = entry.contains("name") ? entry["name"].get<std::string>() : "";
    setStatus("Preset loaded: " + name);
}

void MainWindow::onPresetSave(const std::string& name) {
    if (name.empty()) { setStatus("Enter a preset name before saving"); return; }

    // Capture current engine state
    json data;
    try {
        data = json::parse(engine_->getPresetList());
    } catch (...) {
        setStatus("Failed to capture preset data");
        return;
    }

    // Update existing entry or append new one
    bool found = false;
    for (auto& entry : namedPresets_) {
        if (entry.contains("name") && entry["name"].get<std::string>() == name) {
            entry["data"] = data;
            found = true;
            break;
        }
    }
    if (!found) {
        json entry;
        entry["name"] = name;
        entry["data"] = data;
        namedPresets_.push_back(entry);
    }

    saveNamedPresets();

    // Refresh dropdown
    std::vector<std::string> names;
    for (const auto& p : namedPresets_)
        if (p.contains("name")) names.push_back(p["name"].get<std::string>());
    if (presetBar_) presetBar_->setPresetNames(names);

    setStatus("Preset saved: " + name);
}

void MainWindow::onPresetDelete() {
    if (!presetBar_) return;
    const int idx = presetBar_->getSelectedIndex();
    if (idx < 0 || idx >= static_cast<int>(namedPresets_.size())) {
        setStatus("No preset selected");
        return;
    }
    const std::string name = namedPresets_[static_cast<size_t>(idx)].contains("name")
        ? namedPresets_[static_cast<size_t>(idx)]["name"].get<std::string>() : "";
    namedPresets_.erase(namedPresets_.begin() + idx);
    saveNamedPresets();

    std::vector<std::string> names;
    for (const auto& p : namedPresets_)
        if (p.contains("name")) names.push_back(p["name"].get<std::string>());
    if (presetBar_) {
        presetBar_->setPresetNames(names);
        presetBar_->setCurrentName("");
    }
    setStatus("Preset deleted: " + name);
}