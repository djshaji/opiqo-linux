// tests/audio_engine_stub.cpp
//
// Minimal stub that satisfies the linker when building opiqo_tests
// without GTK headers.  The AudioEngine lifecycle tests construct the
// object and call stop() on an idle engine — they don't need a real
// JACK connection, so all GTK/JACK guts are replaced with no-ops.

#include "gtk4/AudioEngine.h"
#include "LiveEffectEngine.h"

#include <jack/jack.h>
#include <mutex>
#include <string>

// ── Minimal Impl ─────────────────────────────────────────────────────────────
struct AudioEngine::Impl {
    LiveEffectEngine* engine = nullptr;
    AudioEngine*      owner  = nullptr;
    std::function<void(std::string)> errorCb;
    mutable std::mutex errMutex;
    std::string errorMsg;
};

AudioEngine::AudioEngine(LiveEffectEngine* engine) : d_(std::make_unique<Impl>()) {
    d_->engine = engine;
    d_->owner  = this;
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start(const std::string&, const std::string&,
                        const std::string&, const std::string&) {
    // Stub: do nothing — callers only test State::Off / stop()
    return false;
}

void AudioEngine::stop() {
    state_.store(State::Off, std::memory_order_release);
}

std::string AudioEngine::errorMessage() const {
    std::lock_guard<std::mutex> lk(d_->errMutex);
    return d_->errorMsg;
}

void AudioEngine::setErrorCallback(std::function<void(std::string)> cb) {
    d_->errorCb = std::move(cb);
}
