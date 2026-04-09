// src/gtk4/AudioEngine.h
// JACK client lifecycle + real-time DSP dispatch for Opiqo.
//
// Thread ownership:
//   - All public methods:        GTK main thread only
//   - jack_process_callback:     JACK real-time thread only
//   - Cross-thread state:        std::atomic<State>

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class LiveEffectEngine;

class AudioEngine {
public:
    enum class State { Off, Starting, Running, Stopping, Error };

    // engine must outlive this object
    explicit AudioEngine(LiveEffectEngine* engine);
    ~AudioEngine();

    // Start JACK client and connect to the named physical ports.
    // capturePort / playbackPort are full JACK names, e.g. "system:capture_1".
    // Returns true on success; on failure state() == State::Error.
    bool start(const std::string& capturePort,
               const std::string& capturePort2,
               const std::string& playbackPort,
               const std::string& playbackPort2);

    // Stop JACK client cleanly.
    void stop();

    State       state()        const { return state_.load(std::memory_order_acquire); }
    int32_t     sampleRate()   const { return sampleRate_.load(std::memory_order_relaxed); }
    int32_t     blockSize()    const { return blockSize_.load(std::memory_order_relaxed); }
    uint64_t    xrunCount()    const { return xrunCount_.load(std::memory_order_relaxed); }
    std::string errorMessage() const;

    // Optional callback fired on the GTK main thread when an error occurs.
    void setErrorCallback(std::function<void(std::string)> cb);

private:
    struct Impl;
    std::unique_ptr<Impl> d_;

    std::atomic<State>    state_      { State::Off };
    std::atomic<int32_t>  sampleRate_ { 48000 };
    std::atomic<int32_t>  blockSize_  { 1024 };
    std::atomic<uint64_t> xrunCount_  { 0 };
};
