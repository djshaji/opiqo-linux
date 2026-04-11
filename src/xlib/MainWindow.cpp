// src/xlib/MainWindow.cpp — Xlib/Xaw port
// Business logic mirrors src/gtk2/MainWindow.cpp exactly;
// GTK API calls replaced with Xlib/Xaw equivalents.

#include "MainWindow.h"

#include "version.h"
#include "ControlBar.h"
#include "PresetBar.h"
#include "PluginSlot.h"
#include "PluginDialog.h"
#include "SettingsDialog.h"
#include "logging_macros.h"
#include "json.hpp"

#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/AsciiText.h>

#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <cerrno>
#include <cstring>

using json = nlohmann::json;

// ── Helpers ────────────────────────────────────────────────────────────────────

static std::string configDirPath() {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", AppSettings::configPath().c_str());
    return std::string(dirname(tmp));
}

// ── SimpleFileDialog — mini nested-loop file path picker ─────────────────────
// Used for preset export / import paths.

namespace {

struct SimpleFileDlg {
    XtAppContext appCtx;
    Widget       shell    = nullptr;
    Widget       textW    = nullptr;
    std::string  result;
    bool         done     = false;
    bool         accepted = false;

    // Xt callbacks
    static void okCB(Widget, XtPointer client, XtPointer) {
        SimpleFileDlg* d = (SimpleFileDlg*)client;
        char* val = nullptr;
        XtVaGetValues(d->textW, XtNstring, &val, nullptr);
        d->result   = val ? val : "";
        d->accepted = true;
        d->done     = true;
    }
    static void cancelCB(Widget, XtPointer client, XtPointer) {
        ((SimpleFileDlg*)client)->done = true;
    }
};

// Show a simple text-entry dialog.  Blocks (nested event loop).
// Returns the typed path, or "" if cancelled.
static std::string askFilePath(XlibApp& app, Widget parent, const char* title) {
    SimpleFileDlg d;
    d.appCtx = app.appContext();

    d.shell = XtVaCreatePopupShell("fileDialog",
        transientShellWidgetClass, parent,
        XtNtitle, title,
        XtNwidth, 420,
        nullptr);

    Widget form = XtVaCreateManagedWidget("form", formWidgetClass, d.shell,
        XtNdefaultDistance, 8, nullptr);

    Widget lbl = XtVaCreateManagedWidget("lbl", labelWidgetClass, form,
        XtNlabel,     "File path:",
        XtNborderWidth, 0,
        XtNtop,     XawChainTop,
        XtNleft,    XawChainLeft,
        nullptr);

    d.textW = XtVaCreateManagedWidget("text", asciiTextWidgetClass, form,
        XtNwidth,       360,
        XtNeditType,    XawtextEdit,
        XtNfromVert,    lbl,
        XtNtop,         XawChainTop,
        XtNleft,        XawChainLeft,
        nullptr);

    Widget okBtn = XtVaCreateManagedWidget("OK", commandWidgetClass, form,
        XtNfromVert,  d.textW,
        XtNtop,       XawChainTop,
        XtNleft,      XawChainLeft,
        nullptr);
    XtAddCallback(okBtn, XtNcallback, SimpleFileDlg::okCB, &d);

    Widget cancelBtn = XtVaCreateManagedWidget("Cancel", commandWidgetClass, form,
        XtNfromVert,  d.textW,
        XtNfromHoriz, okBtn,
        XtNtop,       XawChainTop,
        XtNleft,      XawChainLeft,
        nullptr);
    XtAddCallback(cancelBtn, XtNcallback, SimpleFileDlg::cancelCB, &d);

    XtPopup(d.shell, XtGrabExclusive);
    XFlush(app.display());

    // Nested event loop
    while (!d.done) {
        XEvent ev;
        XtAppNextEvent(d.appCtx, &ev);
        XtDispatchEvent(&ev);
    }

    XtDestroyWidget(d.shell);
    return d.accepted ? d.result : "";
}

} // anon namespace

// ── Constructor ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(XlibApp& app) : app_(app) {
    engine_   = std::make_unique<LiveEffectEngine>();
    settings_ = AppSettings::load();
    portEnum_ = std::make_unique<JackPortEnum>();
    audioEngine_ = std::make_unique<AudioEngine>(engine_.get());

    // Scan plugins once (or load from cache)
    if (!loadPluginCache()) {
        LOGD("No plugin cache; scanning LV2 plugins...");
        engine_->initPlugins();
        savePluginCache();
    } else {
        LOGD("Plugin cache loaded");
    }

    // JACK port hot-plug
    portEnum_->setChangeCallback([this]() {
        app_.postToMain([this]() {
            setStatus("JACK ports changed — check Settings to update routing");
        });
    });

    // Audio engine error callback — write to self-pipe so Xt wakes up
    audioEngine_->setErrorCallback([this](std::string msg) {
        app_.postToMain([this, m = std::move(msg)]() {
            setStatus("Audio error: " + m);
            controlBar_->setPowerState(false);
        });
    });

    buildWidgets();
    loadNamedPresets();
    startEngineTimer();
}

MainWindow::~MainWindow() {
    if (pollTimerId_) XtRemoveTimeOut(pollTimerId_);
    audioEngine_->stop();
    settings_.save();
}

// ── Widget construction ───────────────────────────────────────────────────────

void MainWindow::buildWidgets() {
    Widget topLevel = app_.topLevel();
    Display* dpy    = app_.display();

    // Root paned widget (vertical stack)
    Widget paned = XtVaCreateManagedWidget("paned",
        panedWidgetClass, topLevel,
        XtNorientation, XtorientVertical,
        nullptr);

    // ── Menu bar (Box of buttons) ─────────────────────────────────────────
    Widget menuBox = XtVaCreateManagedWidget("menuBox",
        boxWidgetClass, paned,
        XtNorientation, XtorientHorizontal,
        XtNborderWidth, 0,
        XtNshowGrip,    False,
        XtNskipAdjust,  True,
        nullptr);

    // File → Quit
    Widget fileBtn = XtVaCreateManagedWidget("File", menuButtonWidgetClass, menuBox,
        XtNmenuName, "fileMenu",
        nullptr);
    Widget fileMenu = XtVaCreatePopupShell("fileMenu",
        simpleMenuWidgetClass, fileBtn, nullptr);
    Widget quitItem = XtVaCreateManagedWidget("Quit", smeBSBObjectClass, fileMenu, nullptr);
    XtAddCallback(quitItem, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<XlibApp*>(client)->quit();
        }, &app_);

    // View → Settings / About
    Widget viewBtn = XtVaCreateManagedWidget("View", menuButtonWidgetClass, menuBox,
        XtNmenuName, "viewMenu",
        nullptr);
    Widget viewMenu = XtVaCreatePopupShell("viewMenu",
        simpleMenuWidgetClass, viewBtn, nullptr);
    Widget settingsItem = XtVaCreateManagedWidget("Settings…",
        smeBSBObjectClass, viewMenu, nullptr);
    XtAddCallback(settingsItem, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            static_cast<MainWindow*>(client)->onSettingsOpen();
        }, this);
    XtVaCreateManagedWidget("line1", smeLineObjectClass, viewMenu, nullptr);
    Widget aboutItem = XtVaCreateManagedWidget("About",
        smeBSBObjectClass, viewMenu, nullptr);
    XtAddCallback(aboutItem, XtNcallback,
        [](Widget, XtPointer client, XtPointer) {
            MainWindow* self = static_cast<MainWindow*>(client);
            // Simple text About dialog
            Widget sh = XtVaCreatePopupShell("about",
                transientShellWidgetClass, self->app_.topLevel(),
                XtNtitle, "About Opiqo", nullptr);
            Widget dlg = XtVaCreateManagedWidget("dialog",
                dialogWidgetClass, sh,
                XtNlabel, "Opiqo " OPIQO_VERSION " \"" CODENAME "\"\n"
                           MOTTO "\n\nBy " AUTHOR,
                nullptr);
            XawDialogAddButton(dlg, "Close",
                [](Widget, XtPointer p, XtPointer) { XtDestroyWidget((Widget)p); },
                sh);
            XtPopup(sh, XtGrabNonexclusive);
        }, this);

    // ── 2×2 plugin slot grid ──────────────────────────────────────────────
    slotGrid_ = XtVaCreateManagedWidget("slotGrid",
        formWidgetClass, paned,
        XtNdefaultDistance, 6,
        nullptr);

    // Place 4 slots: 0=(tl), 1=(tr), 2=(bl), 3=(br)
    for (int i = 0; i < 4; ++i) {
        slots_[i] = std::make_unique<PluginSlot>(i + 1, slotGrid_, app_);
        Widget w = slots_[i]->widget();

        // Constrain within Form
        bool rightCol = (i % 2 == 1);
        bool bottomRow= (i / 2 == 1);

        if (!rightCol && !bottomRow) {
            // top-left — anchor to form edges
            XtVaSetValues(w,
                XtNtop,    XawChainTop,
                XtNleft,   XawChainLeft,
                XtNbottom, XawRubber,
                XtNright,  XawRubber,
                nullptr);
        } else if (rightCol && !bottomRow) {
            XtVaSetValues(w,
                XtNfromHoriz, slots_[0]->widget(),
                XtNtop,       XawChainTop,
                XtNleft,      XawRubber,
                XtNbottom,    XawRubber,
                XtNright,     XawChainRight,
                nullptr);
        } else if (!rightCol && bottomRow) {
            XtVaSetValues(w,
                XtNfromVert, slots_[0]->widget(),
                XtNtop,      XawRubber,
                XtNleft,     XawChainLeft,
                XtNbottom,   XawChainBottom,
                XtNright,    XawRubber,
                nullptr);
        } else {
            XtVaSetValues(w,
                XtNfromHoriz, slots_[2]->widget(),
                XtNfromVert,  slots_[1]->widget(),
                XtNtop,       XawRubber,
                XtNleft,      XawRubber,
                XtNbottom,    XawChainBottom,
                XtNright,     XawChainRight,
                nullptr);
        }

        // Wire slot callbacks
        slots_[i]->setAddCallback   ([this](int s)             { onAddPlugin(s); });
        slots_[i]->setDeleteCallback([this](int s)             { onDeletePlugin(s); });
        slots_[i]->setBypassCallback([this](int s, bool b)     { onBypassPlugin(s, b); });
        slots_[i]->setSetValueCallback([this](int s, uint32_t p, float v) {
            onSetValue(s, p, v);
        });
        slots_[i]->setSetFileCallback([this](int s, uint32_t p, const std::string& path) {
            onSetFilePath(s, p, path);
        });
    }

    // ── Preset bar ────────────────────────────────────────────────────────
    Widget separator1 = XtVaCreateManagedWidget("sep1",
        labelWidgetClass, paned,
        XtNlabel,       "",
        XtNborderWidth, 0,
        XtNheight,      2,
        XtNshowGrip,    False,
        XtNskipAdjust,  True,
        nullptr);
    (void)separator1;

    presetBar_ = std::make_unique<PresetBar>(paned, app_);
    presetBar_->setLoadCallback  ([this]()                    { onPresetLoad(); });
    presetBar_->setSaveCallback  ([this](const std::string& n){ onPresetSave(n); });
    presetBar_->setDeleteCallback([this]()                    { onPresetDelete(); });

    // ── Control bar ───────────────────────────────────────────────────────
    controlBar_ = std::make_unique<ControlBar>(paned, app_);
    controlBar_->setPowerCallback ([this](bool on)              { onPowerToggled(on); });
    controlBar_->setGainCallback  ([this](float g)              { onGainChanged(g); });
    controlBar_->setRecordCallback([this](bool s, int f, int q) { onRecordToggled(s, f, q); });
}

// ── Engine poll timer ─────────────────────────────────────────────────────────

void MainWindow::startEngineTimer() {
    pollTimerId_ = app_.addTimer(200, enginePollCB, this);
}

/*static*/
void MainWindow::enginePollCB(XtPointer client, XtIntervalId* id) {
    MainWindow* self = static_cast<MainWindow*>(client);
    self->pollEngineState();
    // Reschedule (one-shot timer)
    self->pollTimerId_ = self->app_.addTimer(200, enginePollCB, self);
}

void MainWindow::pollEngineState() {
    if (!audioEngine_) return;
    const auto state = audioEngine_->state();
    if (state == AudioEngine::State::Error) {
        controlBar_->setPowerState(false);
    }
    controlBar_->setXrunCount(audioEngine_->xrunCount());
}

// ── Power / settings ──────────────────────────────────────────────────────────

void MainWindow::onPowerToggled(bool on) {
    if (on) {
        auto caps  = portEnum_->enumerateCapturePorts();
        auto plays = portEnum_->enumeratePlaybackPorts();

        const std::string cap1 = JackPortEnum::resolveOrDefault(caps,  settings_.capturePort);
        const std::string cap2 = JackPortEnum::resolveOrDefault(caps,  settings_.capturePort2);
        const std::string pb1  = JackPortEnum::resolveOrDefault(plays, settings_.playbackPort);
        const std::string pb2  = JackPortEnum::resolveOrDefault(plays, settings_.playbackPort2);

        if (!audioEngine_->start(cap1, cap2, pb1, pb2)) {
            controlBar_->setPowerState(false);
            return;
        }
        setStatus("Engine running  •  " +
                  std::to_string(audioEngine_->sampleRate()) + " Hz  •  " +
                  std::to_string(audioEngine_->blockSize())  + " frames/block");
    } else {
        audioEngine_->stop();
        setStatus("Engine stopped");
    }
}

void MainWindow::onGainChanged(float gain) {
    if (engine_->gain) *engine_->gain = gain;
    settings_.gain = gain;
}

void MainWindow::onSettingsOpen() {
    if (!settingsDlg_) {
        const auto caps  = portEnum_->enumerateCapturePorts();
        const auto plays = portEnum_->enumeratePlaybackPorts();

        settingsDlg_ = std::make_unique<SettingsDialog>(
            app_.topLevel(), app_, settings_, caps, plays);

        settingsDlg_->setApplyCallback ([this](const AppSettings& s){ onSettingsApply(s); });
        settingsDlg_->setExportCallback([this]()                     { onExportPreset(); });
        settingsDlg_->setImportCallback([this](const std::string& p) { onImportPreset(p); });
        settingsDlg_->setDeleteCacheCallback([this]() {
            std::string path = configDirPath() + "/opiqo_plugin_cache.json";
            unlink(path.c_str());
            setStatus("Plugin cache deleted");
        });
    }
    settingsDlg_->show();
}

void MainWindow::onSettingsApply(const AppSettings& s) {
    settings_ = s;
    const bool wasRunning = (audioEngine_->state() == AudioEngine::State::Running);
    if (wasRunning) {
        audioEngine_->stop();
        onPowerToggled(true);
    }
    settings_.save();
}

// ── Recording ─────────────────────────────────────────────────────────────────

void MainWindow::onRecordToggled(bool start, int format, int quality) {
    if (start) {
        if (audioEngine_->state() != AudioEngine::State::Running) {
            setStatus("Start the engine before recording");
            controlBar_->setRecordingActive(false);
            return;
        }

        const char* musicDir = getenv("XDG_MUSIC_DIR");
        if (!musicDir || musicDir[0] == '\0') musicDir = getenv("HOME");
        if (!musicDir) musicDir = "/tmp";

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
        setStatus("Recording → " + path);
    } else {
        engine_->stopRecording();
        setStatus("Recording stopped");
    }
}

// ── Plugin management ─────────────────────────────────────────────────────────

void MainWindow::onAddPlugin(int slot) {
    if (audioEngine_->state() != AudioEngine::State::Running) {
        setStatus("Start the engine before loading plugins");
        return;
    }

    const json plugins = engine_->getAvailablePlugins();
    if (plugins.empty()) {
        setStatus("No LV2 plugins found. Install plugins and restart.");
        return;
    }

    const std::string pluginsJson = plugins.dump();
    auto* dlg = new PluginDialog(app_.topLevel(), app_, pluginsJson);
    dlg->show([this, slot](const std::string& uri) {
        const int rc = engine_->addPlugin(slot, uri);
        if (rc != 0) {
            setStatus("Failed to load plugin: " + uri);
        } else {
            const json all = engine_->getAvailablePlugins();
            std::string name = uri;
            if (all.contains(uri) && all[uri].contains("name"))
                name = all[uri]["name"].get<std::string>();

            const auto ports = engine_->getPluginPortInfo(slot);
            slots_[slot - 1]->onPluginAdded(name, ports);
            setStatus("Loaded: " + name);
        }
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

void MainWindow::onSetFilePath(int slot, uint32_t portIndex,
                               const std::string& path) {
    // engine_->setFilePath expects (slot, uri, path) in gtk2 — but here we have portIdx
    // dispatch through the correct engine API
    engine_->setFilePath(slot, std::to_string(portIndex), path);
}

// ── Status bar ────────────────────────────────────────────────────────────────

void MainWindow::setStatus(const std::string& msg) {
    LOGD("[status] %s", msg.c_str());
    if (controlBar_) controlBar_->setStatusText(msg);
}

// ── Plugin cache ──────────────────────────────────────────────────────────────

void MainWindow::savePluginCache() {
    const json cache = engine_->getAvailablePlugins();
    const std::string dir  = configDirPath();
    const std::string path = dir + "/opiqo_plugin_cache.json";

    mkdir(dir.c_str(), 0755);
    std::ofstream f(path);
    if (f.is_open()) {
        f << cache.dump(4);
        LOGD("Plugin cache saved: %s", path.c_str());
    } else {
        LOGD("Cannot save plugin cache: %s", path.c_str());
    }
}

bool MainWindow::loadPluginCache() {
    const std::string path = configDirPath() + "/opiqo_plugin_cache.json";
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try {
        json cache;
        f >> cache;
        engine_->pluginInfo = cache;
        lilv_world_load_all(engine_->world);
        engine_->plugins = lilv_world_get_all_plugins(engine_->world);
        LOGD("Plugin cache loaded from %s", path.c_str());
        return true;
    } catch (...) {
        return false;
    }
}

// ── Named preset management ───────────────────────────────────────────────────

std::string MainWindow::namedPresetsPath() const {
    return configDirPath() + "/opiqo_named_presets.json";
}

void MainWindow::loadNamedPresets() {
    std::ifstream f(namedPresetsPath());
    if (!f.is_open()) return;
    try {
        json j;
        f >> j;
        if (j.is_array()) namedPresets_ = j;

        std::vector<std::string> names;
        for (const auto& p : namedPresets_)
            if (p.contains("name") && p["name"].is_string())
                names.push_back(p["name"].get<std::string>());
        if (presetBar_) presetBar_->setPresetNames(names);
    } catch (...) {}
}

void MainWindow::saveNamedPresets() {
    const std::string dir = configDirPath();
    mkdir(dir.c_str(), 0755);
    std::ofstream f(namedPresetsPath());
    if (f.is_open()) f << namedPresets_.dump(2) << '\n';
}

void MainWindow::applyFullPreset(const nlohmann::json& presetData) {
    if (presetData.contains("gain") && presetData["gain"].is_number()) {
        const float g = presetData["gain"].get<float>();
        if (engine_->gain) *engine_->gain = g;
        settings_.gain = g;
        if (controlBar_) controlBar_->setGainValue(g);
    }

    const json all = engine_->getAvailablePlugins();
    for (int slot = 1; slot <= 4; ++slot) {
        const std::string key = "plugin" + std::to_string(slot);
        if (!presetData.contains(key)) continue;
        const json& sp = presetData[key];

        engine_->deletePlugin(slot);
        slots_[slot - 1]->onPluginCleared();

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
    if (idx < 0 || idx >= (int)namedPresets_.size()) {
        setStatus("No preset selected"); return;
    }
    const json& entry = namedPresets_[(size_t)idx];
    if (!entry.contains("data")) { setStatus("Invalid preset data"); return; }
    applyFullPreset(entry["data"]);
    setStatus("Preset loaded: " +
              (entry.contains("name") ? entry["name"].get<std::string>() : ""));
}

void MainWindow::onPresetSave(const std::string& name) {
    if (name.empty()) { setStatus("Enter a preset name before saving"); return; }

    json data;
    try { data = json::parse(engine_->getPresetList()); }
    catch (...) { setStatus("Failed to capture preset data"); return; }

    bool found = false;
    for (auto& entry : namedPresets_) {
        if (entry.contains("name") && entry["name"].get<std::string>() == name) {
            entry["data"] = data; found = true; break;
        }
    }
    if (!found) {
        json e; e["name"] = name; e["data"] = data;
        namedPresets_.push_back(e);
    }

    saveNamedPresets();

    std::vector<std::string> names;
    for (const auto& p : namedPresets_)
        if (p.contains("name")) names.push_back(p["name"].get<std::string>());
    if (presetBar_) presetBar_->setPresetNames(names);
    setStatus("Preset saved: " + name);
}

void MainWindow::onPresetDelete() {
    if (!presetBar_) return;
    const int idx = presetBar_->getSelectedIndex();
    if (idx < 0 || idx >= (int)namedPresets_.size()) {
        setStatus("No preset selected"); return;
    }
    std::string name = namedPresets_[(size_t)idx].contains("name")
                       ? namedPresets_[(size_t)idx]["name"].get<std::string>() : "";
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

void MainWindow::onExportPreset() {
    const std::string path = askFilePath(app_, app_.topLevel(), "Export preset — enter path");
    if (path.empty()) return;

    std::string data;
    try { data = engine_->getPresetList(); }
    catch (...) { setStatus("Failed to serialize preset"); return; }

    std::ofstream f(path);
    if (f.is_open()) { f << data; setStatus("Preset exported to " + path); }
    else setStatus("Cannot write to: " + path);
}

void MainWindow::onImportPreset(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) { setStatus("Cannot open preset file"); return; }

    try {
        json j; f >> j;
        if (j.is_array()) {
            for (int slot = 1; slot <= 4 && (slot - 1) < (int)j.size(); ++slot) {
                engine_->applyPreset(slot, j[slot - 1]);
                const json all = engine_->getAvailablePlugins();
                const json& sp = j[slot - 1];
                if (engine_->getPluginPortInfo(slot).empty()) {
                    slots_[slot - 1]->onPluginCleared();
                } else {
                    std::string name = "Plugin (slot " + std::to_string(slot) + ")";
                    if (sp.contains("uri") && all.contains(sp["uri"].get<std::string>()))
                        name = all[sp["uri"].get<std::string>()]["name"].get<std::string>();
                    slots_[slot - 1]->onPluginAdded(name, engine_->getPluginPortInfo(slot));
                }
            }
        }
        setStatus("Preset imported");
    } catch (...) { setStatus("Failed to parse preset file"); }
}
