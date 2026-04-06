// src/linux/JackPortEnum.cpp

#include "JackPortEnum.h"

#include <gtk/gtk.h>       // g_idle_add
#include <jack/jack.h>

#include <algorithm>
#include <cstring>

// ── Implementation details ────────────────────────────────────────────────────

struct JackPortEnum::Impl {
    jack_client_t*        client   = nullptr;
    std::function<void()> changeCb;

    // JACK port-registration callback fires on the JACK thread; marshal to GTK.
    static void portRegistrationCb(jack_port_id_t /*id*/, int /*reg*/, void* arg) {
        auto* self = static_cast<Impl*>(arg);
        if (!self->changeCb) return;

        // Heap-allocate the callback so g_idle_add can own it
        auto* cb = new std::function<void()>(self->changeCb);
        g_idle_add([](gpointer data) -> gboolean {
            auto* fn = static_cast<std::function<void()>*>(data);
            (*fn)();
            delete fn;
            return G_SOURCE_REMOVE;
        }, cb);
    }
};

// ── JackPortEnum ─────────────────────────────────────────────────────────────

JackPortEnum::JackPortEnum() : d_(new Impl) {
    jack_status_t status;
    // Use a unique temporary client name to avoid conflicts
    d_->client = jack_client_open("opiqo-enum", JackNoStartServer, &status);
    if (!d_->client) return;

    jack_set_port_registration_callback(d_->client,
                                        Impl::portRegistrationCb, d_);
    jack_activate(d_->client);
}

JackPortEnum::~JackPortEnum() {
    if (d_->client) {
        jack_deactivate(d_->client);
        jack_client_close(d_->client);
    }
    delete d_;
}

void JackPortEnum::setChangeCallback(std::function<void()> cb) {
    d_->changeCb = std::move(cb);
}

std::vector<PortInfo> JackPortEnum::enumerateCapturePorts() {
    std::vector<PortInfo> result;
    if (!d_->client) return result;

    // Physical input ports (from the perspective of JACK) that supply audio to clients
    const char** ports = jack_get_ports(d_->client, nullptr,
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsOutput | JackPortIsPhysical);
    if (!ports) return result;

    bool first = true;
    for (int i = 0; ports[i]; ++i) {
        PortInfo info;
        info.id          = ports[i];
        // Strip client prefix for display: "system:capture_1" → "capture_1"
        const char* colon = std::strchr(ports[i], ':');
        info.friendlyName = colon ? (colon + 1) : ports[i];
        info.isDefault    = first;
        result.push_back(std::move(info));
        first = false;
    }
    jack_free(ports);
    return result;
}

std::vector<PortInfo> JackPortEnum::enumeratePlaybackPorts() {
    std::vector<PortInfo> result;
    if (!d_->client) return result;

    // Physical output ports (sink for clients)
    const char** ports = jack_get_ports(d_->client, nullptr,
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsInput | JackPortIsPhysical);
    if (!ports) return result;

    bool first = true;
    for (int i = 0; ports[i]; ++i) {
        PortInfo info;
        info.id          = ports[i];
        const char* colon = std::strchr(ports[i], ':');
        info.friendlyName = colon ? (colon + 1) : ports[i];
        info.isDefault    = first;
        result.push_back(std::move(info));
        first = false;
    }
    jack_free(ports);
    return result;
}

/*static*/ std::string JackPortEnum::resolveOrDefault(
        const std::vector<PortInfo>& ports, const std::string& saved) {
    for (const auto& p : ports)
        if (p.id == saved) return p.id;
    return ports.empty() ? "" : ports.front().id;
}
