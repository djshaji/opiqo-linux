// src/xlib/AudioEngine.cpp  (GTK-free version for the Xlib frontend)
// g_idle_add replaced by direct calls; MainWindow wraps callbacks in postToMain()

#include "AudioEngine.h"
#include "LiveEffectEngine.h"
#include "logging_macros.h"

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
    std::vector<float> interleavedIn;
    std::vector<float> interleavedOut;

    // ── Pending buffer-size change (written by RT bufferSizeCb, cleared by main) ─
    std::atomic<uint32_t> pendingBufSize_{0};

    // ── Error string ──────────────────────────────────────────────────────────
    mutable std::mutex  errMutex;
    std::string         errorMsg;

    // ── JACK callbacks (static) ───────────────────────────────────────────────

    static int processCb(jack_nframes_t nframes, void* arg) {
        auto* d = static_cast<Impl*>(arg);

        auto* inL  = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->capL, nframes));
        auto* inR  = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->capR, nframes));
        auto* outL = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->pbkL, nframes));
        auto* outR = static_cast<jack_default_audio_sample_t*>(
                         jack_port_get_buffer(d->pbkR, nframes));

        if (!inL || !inR || !outL || !outR) return 0;

        if (d->pendingBufSize_.load(std::memory_order_acquire) != 0) {
            memcpy(outL, inL, sizeof(float) * nframes);
            memcpy(outR, inR, sizeof(float) * nframes);
            return 0;
        }

        const jack_nframes_t n = nframes;

        float* si = d->interleavedIn.data();
        for (jack_nframes_t i = 0; i < n; ++i) {
            si[i * 2]     = inL[i];
            si[i * 2 + 1] = inR[i];
        }

        d->engine->process(d->interleavedIn.data(),
                           d->interleavedOut.data(),
                           static_cast<int>(n));

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

    // Called from JACK server thread (not RT audio thread) — safe to allocate.
    static int bufferSizeCb(jack_nframes_t nframes, void* arg) {
        auto* d = static_cast<Impl*>(arg);
        d->owner->blockSize_.store(static_cast<int32_t>(nframes),
                                   std::memory_order_relaxed);
        d->pendingBufSize_.store(static_cast<uint32_t>(nframes),
                                 std::memory_order_release);

        // Reallocate inline; JACK's buffer-size callback is not the RT thread.
        if (d->owner->state_.load(std::memory_order_acquire) == AudioEngine::State::Running) {
            const size_t newSamples = static_cast<size_t>(nframes) * 2;
            d->interleavedIn.assign(newSamples, 0.0f);
            d->interleavedOut.assign(newSamples, 0.0f);
            d->engine->blockSize = static_cast<int32_t>(nframes);
            LOGD("[AudioEngine] work buffers reallocated to %u frames", nframes);
        }
        d->pendingBufSize_.store(0, std::memory_order_release);
        return 0;
    }

    static void serverLostCb(void* arg) {
        auto* d = static_cast<Impl*>(arg);
        d->setError("JACK server connection lost");
        d->owner->state_.store(AudioEngine::State::Error,
                               std::memory_order_release);
    }

    // Called from JACK thread; MainWindow wraps errorCb in app_.postToMain().
    void setError(const std::string& msg) {
        {
            std::lock_guard<std::mutex> lk(errMutex);
            errorMsg = msg;
        }
        if (errorCb) errorCb(msg);
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

    const jack_nframes_t sr  = jack_get_sample_rate(d_->client);
    const jack_nframes_t bs  = jack_get_buffer_size(d_->client);
    sampleRate_.store(static_cast<int32_t>(sr),  std::memory_order_relaxed);
    blockSize_.store( static_cast<int32_t>(bs),  std::memory_order_relaxed);

    d_->engine->sampleRate = static_cast<int32_t>(sr);
    d_->engine->blockSize  = static_cast<int32_t>(bs);

    const size_t bufSamples = static_cast<size_t>(bs) * 2;
    d_->interleavedIn.assign( bufSamples, 0.0f);
    d_->interleavedOut.assign(bufSamples, 0.0f);

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

    jack_set_process_callback(d_->client,     Impl::processCb,    d_.get());
    jack_set_xrun_callback(d_->client,        Impl::xrunCb,       this);
    jack_set_buffer_size_callback(d_->client, Impl::bufferSizeCb, d_.get());
    jack_on_shutdown(d_->client,              Impl::serverLostCb, d_.get());

    if (jack_activate(d_->client) != 0) {
        d_->setError("jack_activate() failed");
        jack_client_close(d_->client);
        d_->client = nullptr;
        state_.store(State::Error, std::memory_order_release);
        return false;
    }

    auto tryConnect = [&](const std::string& src, const std::string& dst) {
        if (src.empty() || dst.empty()) return;
        if (jack_connect(d_->client, src.c_str(), dst.c_str()) != 0)
            LOGW("Could not connect %s → %s", src.c_str(), dst.c_str());
    };

    const std::string capL_full = std::string(jack_port_name(d_->capL));
    const std::string capR_full = std::string(jack_port_name(d_->capR));
    const std::string pbkL_full = std::string(jack_port_name(d_->pbkL));
    const std::string pbkR_full = std::string(jack_port_name(d_->pbkR));

    tryConnect(capturePort,  capL_full);
    tryConnect(capturePort2.empty() ? capturePort : capturePort2, capR_full);
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

    d_->interleavedIn.clear();
    d_->interleavedIn.shrink_to_fit();
    d_->interleavedOut.clear();
    d_->interleavedOut.shrink_to_fit();

    state_.store(State::Off, std::memory_order_release);
    LOGD("[AudioEngine] stopped");
}
