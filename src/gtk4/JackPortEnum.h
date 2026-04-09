// src/gtk4/JackPortEnum.h
// Discovers physical JACK capture/playback ports and watches for hot-plug events.
// Opens a short-lived JACK client for probing; does NOT own the audio pipeline.

#pragma once

#include <functional>
#include <string>
#include <vector>

struct PortInfo {
    std::string id;            // JACK port name, e.g. "system:capture_1"
    std::string friendlyName;  // Short display name, e.g. "capture_1"
    bool        isDefault = false;
};

class JackPortEnum {
public:
    JackPortEnum();
    ~JackPortEnum();

    // Discover available ports.  Call after construction or after a change callback fires.
    std::vector<PortInfo> enumerateCapturePorts();
    std::vector<PortInfo> enumeratePlaybackPorts();

    // Fired on the GTK main thread (via g_idle_add) when ports are registered/unregistered.
    void setChangeCallback(std::function<void()> cb);

    // Resolve a saved port name:
    //   * if saved name is found in list  → return it
    //   * otherwise                       → return first port in list, or "" if empty
    static std::string resolveOrDefault(const std::vector<PortInfo>& ports,
                                        const std::string& saved);

private:
    struct Impl;
    Impl* d_ = nullptr;
};
