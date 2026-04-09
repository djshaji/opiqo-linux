// src/gtk4/AppSettings.cpp

#include "AppSettings.h"
#include "json.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

using json = nlohmann::json;

// ── helpers ──────────────────────────────────────────────────────────────────

static void mkdirp(const std::string& path) {
    std::string tmp;
    for (char c : path) {
        tmp += c;
        if (c == '/') mkdir(tmp.c_str(), 0755);
    }
}

// ── AppSettings ──────────────────────────────────────────────────────────────

std::string AppSettings::configPath() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && xdg[0] != '\0') {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = home ? std::string(home) + "/.config" : "/tmp";
    }
    return base + "/opiqo/settings.json";
}

AppSettings AppSettings::load() {
    AppSettings s;
    const std::string path = configPath();
    std::ifstream f(path);
    if (!f.is_open()) return s;   // file absent — use defaults

    try {
        json j;
        f >> j;

        if (j.contains("capturePort")   && j["capturePort"].is_string())
            s.capturePort   = j["capturePort"].get<std::string>();
        if (j.contains("capturePort2")  && j["capturePort2"].is_string())
            s.capturePort2  = j["capturePort2"].get<std::string>();
        if (j.contains("playbackPort")  && j["playbackPort"].is_string())
            s.playbackPort  = j["playbackPort"].get<std::string>();
        if (j.contains("playbackPort2") && j["playbackPort2"].is_string())
            s.playbackPort2 = j["playbackPort2"].get<std::string>();
        if (j.contains("recordFormat")  && j["recordFormat"].is_number_integer())
            s.recordFormat  = j["recordFormat"].get<int>();
        if (j.contains("recordQuality") && j["recordQuality"].is_number_integer())
            s.recordQuality = j["recordQuality"].get<int>();
        if (j.contains("gain") && j["gain"].is_number())
            s.gain          = j["gain"].get<float>();
    } catch (...) {
        // Corrupt file — silently return defaults
    }
    return s;
}

void AppSettings::save() const {
    const std::string path = configPath();
    // Ensure directory exists
    mkdirp(path.substr(0, path.rfind('/')));

    json j;
    j["capturePort"]   = capturePort;
    j["capturePort2"]  = capturePort2;
    j["playbackPort"]  = playbackPort;
    j["playbackPort2"] = playbackPort2;
    j["recordFormat"]  = recordFormat;
    j["recordQuality"] = recordQuality;
    j["gain"]          = gain;

    std::ofstream f(path);
    if (f.is_open()) f << j.dump(2) << '\n';
}
