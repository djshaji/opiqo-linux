// src/linux/AudioEngine.cpp

#include "AudioEngine.h"
#include "LiveEffectEngine.h"
#include "logging_macros.h"

#include <gtk/gtk.h>       // g_idle_add
#include <jack/jack.h>

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// ── Implementation struct ──────────────────────────────────────────────────────

struct AudioEngine::Impl {
    // ── Owned by constructor ─────────────────────────────────────────────────
    LiveEffectEngine*          engine       = nullptr;
    AudioEngine*               owner        = nullptr;
    std::function<void(std::string)> errorCb;

    // ── JACK objects (valid between start() and stop()) ──────────────────────
    jack_client_t* client  = nullptr;
    jack_port_t*   capL    = nullptr;
    jack_port_t*   capR    = nullptr;
    jack_port_t*   pbkL    = nullptr;
    jack_port_t*   pbkR    = nullptr;

    // ── Pre-allocated interleaved stereo work buffers (alloc in start()) ─────
    // Size: 2 * max_block_frames * sizeof(float)
    std::vector<float> interleavedIn;
    std::vector<float> interleavedOut;

    // ── Error string (written from JACK thread via g_idle_add, read on main) ─
    mutable std::mutex  errMutex;
    std::string         errorMsg;

    // ── JACK callbacks (static, called on RT thread) ─────────────────────────

    static int processCb(jack_nframes_t nframes, void* arg) {
        auto* d = static_cast<Impl*>(arg);

        // Retrieve JACK port buffers (non-interleaved, float32)
        auto* inL  = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->capL, nframes));
        auto* inR  = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->capR, nframes));
        auto* outL = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->pbkL, nframes));
        auto* outR = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->pbkR, nframes));

        if (!inL || !inR || !outL || !outR) return 0;

        const jack_nframes_t n = nframes;

        // Interleave: JACK non-interleaved → LiveEffectEngine interleaved stereo
        float* si = d->interleavedIn.data();
        for (jack_nframes_t i = 0; i < n; ++i) {
            si[i * 2]     = inL[i];
            si[i * 2 + 1] = inR[i];
        }

        // DSP
        d->engine->process(d->interleavedIn.data(),
                           d->interleavedOut.data(),
                           static_cast<int>(n));

        // Deinterleave: LiveEffectEngine interleaved stereo → JACK non-interleaved
        const float* so = d->interleavedOut.data();
        for (jack_nframes_t i = 0; i < n; ++i) {
            outL[i] = so[i * 2];
            outR[i] = so[i * 2 + 1];
        }

        return 0;
    }

    static int xrunCb(void* arg) {
        auto* owner_ptr = static_cast<AudioEngine*>(arg);
        owner_ptr->xrunCount_.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    static int bufferSizeCb(jack_nframes_t nframes, void* arg) {
        auto* owner_ptr = static_cast<AudioEngine*>(arg);
        owner_ptr->blockSize_.store(static_cast<int32_t>(nframes),
                                    std::memory_order_relaxed);
        return 0;
    }

    static void serverLostCb(void* arg) {
        auto* d = static_cast<Impl*>(arg);
        d->setError("JACK server connection lost");
        d->owner->state_.store(AudioEngine::State::Error,
                               std::memory_order_release);
    }

    // RT-safe: no GTK calls here; marshal via g_idle_add
    void setError(const std::string& msg) {
        {
            std::lock_guard<std::mutex> lk(errMutex);
            errorMsg = msg;
        }
        if (!errorCb) return;
        struct Payload { Impl* d; std::string msg; };
        auto* p = new Payload{this, msg};
        g_idle_add([](gpointer data) -> gboolean {
            auto* p = static_cast<Payload*>(data);
            if (p->d->errorCb) p->d->errorCb(p->msg);
            delete p;
            return G_SOURCE_REMOVE;
        }, p);
    }
};

// ── AudioEngine ───────────────────────────────────────────────────────────────

AudioEngine::AudioEngine(LiveEffectEngine* engine)
    : d_(std::make_unique<Impl>()) {
    d_->engine = engine;
    d_->owner  = this;
}

AudioEngine::~AudioEngine() {
    if (state_.load() != State::Off)
        stop();
}

void AudioEngine::setErrorCallback(std::function<void(std::string)> cb) {
    d_->errorCb = std::move(cb);
}

std::string AudioEngine::errorMessage() const {
    std::lock_guard<std::mutex> lk(d_->errMutex);
    return d_->errorMsg;
}

bool AudioEngine::start(const std::string& capturePort,
                        const std::string& capturePort2,
                        const std::string& playbackPort,
                        const std::string& playbackPort2) {
    if (state_.load() != State::Off) return false;
    state_.store(State::Starting, std::memory_order_release);

    jack_status_t status = {};
    d_->client = jack_client_open("opiqo", JackNoStartServer, &status);
    if (!d_->client) {
        d_->setError("Cannot connect to JACK server (is jackd / pipewire-jack running?)");
        state_.store(State::Error, std::memory_order_release);
        return false;
    }

    // ── Read server parameters ────────────────────────────────────────────
    const jack_nframes_t sr  = jack_get_sample_rate(d_->client);
    const jack_nframes_t bs  = jack_get_buffer_size(d_->client);
    sampleRate_.store(static_cast<int32_t>(sr),  std::memory_order_relaxed);
    blockSize_.store( static_cast<int32_t>(bs),  std::memory_order_relaxed);

    // Propagate sample rate and block size to the DSP engine
    d_->engine->sampleRate = static_cast<int32_t>(sr);
    d_->engine->blockSize  = static_cast<int32_t>(bs);

    // ── Pre-allocate RT work buffers (zeroed) ─────────────────────────────
    // max_bs * 2 channels; multiply by 4 to handle temporary blockSize increases
    const size_t bufSamples = static_cast<size_t>(bs) * 2 * 4;
    d_->interleavedIn.assign( bufSamples, 0.0f);
    d_->interleavedOut.assign(bufSamples, 0.0f);

    // ── Register JACK ports ───────────────────────────────────────────────
    d_->capL = jack_port_register(d_->client, "capture_1",
                                  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    d_->capR = jack_port_register(d_->client, "capture_2",
                                  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    d_->pbkL = jack_port_register(d_->client, "playback_1",
                                  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    d_->pbkR = jack_port_register(d_->client, "playback_2",
                                  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (!d_->capL || !d_->capR || !d_->pbkL || !d_->pbkR) {
        d_->setError("Failed to register JACK ports");
        jack_client_close(d_->client);
        d_->client = nullptr;
        state_.store(State::Error, std::memory_order_release);
        return false;
    }

    // ── Register callbacks ────────────────────────────────────────────────
    jack_set_process_callback(d_->client,     Impl::processCb,    d_.get());
    jack_set_xrun_callback(d_->client,        Impl::xrunCb,       this);
    jack_set_buffer_size_callback(d_->client, Impl::bufferSizeCb, this);
    jack_on_shutdown(d_->client,              Impl::serverLostCb, d_.get());

    // ── Activate ──────────────────────────────────────────────────────────
    if (jack_activate(d_->client) != 0) {
        d_->setError("jack_activate() failed");
        jack_client_close(d_->client);
        d_->client = nullptr;
        state_.store(State::Error, std::memory_order_release);
        return false;
    }

    // ── Connect to physical ports (best-effort) ───────────────────────────
    auto tryConnect = [&](const std::string& src, const std::string& dst) {
        if (src.empty() || dst.empty()) return;
        if (jack_connect(d_->client, src.c_str(), dst.c_str()) != 0)
            LOGW("Could not connect %s → %s", src.c_str(), dst.c_str());
    };

    // Capture: physical output → our input ports
    const std::string capL_full = std::string(jack_port_name(d_->capL));
    const std::string capR_full = std::string(jack_port_name(d_->capR));
    const std::string pbkL_full = std::string(jack_port_name(d_->pbkL));
    const std::string pbkR_full = std::string(jack_port_name(d_->pbkR));

    tryConnect(capturePort,  capL_full);
    tryConnect(capturePort2.empty() ? capturePort : capturePort2, capR_full);

    // Playback: our output ports → physical input
    tryConnect(pbkL_full, playbackPort);
    tryConnect(pbkR_full, playbackPort2.empty() ? playbackPort : playbackPort2);

    state_.store(State::Running, std::memory_order_release);
    LOGD("[AudioEngine] started – SR=%d bs=%d", (int)sr, (int)bs);
    return true;
}

void AudioEngine::stop() {
    if (state_.load() == State::Off) return;
    state_.store(State::Stopping, std::memory_order_release);

    if (d_->client) {
        jack_deactivate(d_->client);
        jack_client_close(d_->client);
        d_->client = nullptr;
    }
    d_->capL = d_->capR = d_->pbkL = d_->pbkR = nullptr;

    // Release work buffers
    d_->interleavedIn.clear();
    d_->interleavedIn.shrink_to_fit();
    d_->interleavedOut.clear();
    d_->interleavedOut.shrink_to_fit();

    state_.store(State::Off, std::memory_order_release);
    LOGD("[AudioEngine] stopped");
}
