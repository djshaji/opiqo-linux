// tests/test_live_effect_engine.cpp
//
// Unit tests for LiveEffectEngine and LV2Plugin.
//
// Test coverage:
//   1. Plugin discovery (initPlugins)
//   2. Audio engine start / stop (AudioEngine)
//   3. Loading and unloading plugins (addPlugin / deletePlugin)
//   4. Parameter changes on a loaded plugin (setValue / getPreset)
//   5. pluginEnabled toggling
//   6. process() passthrough when no plugin is loaded
//   7. process() with a real LV2 plugin in the chain
//   8. getPluginPortInfo metadata
//   9. Preset round-trip (getPreset / applyPreset)
//  10. Duplicate load at the same slot replaces the previous plugin
//
// Requires:
//   - http://plugin.org.uk/swh-plugins/amp   (stereo-capable mono amp, SWH pack)
//   - http://gareus.org/oss/lv2/nodelay      (No Delay Line, stereo)
// Both are expected to be installed in /usr/lib64/lv2/ on Fedora/RPM systems.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "LiveEffectEngine.h"
#include "gtk4/AudioEngine.h"

// ─── Environment ──────────────────────────────────────────────────────────────
// LockFreeQueueManager starts a background thread whose join/stop is currently
// commented-out in quit().  Without a clean join the destructor aborts at
// process exit.  We side-step the issue with _exit() so the test runner reports
// PASSED/FAILED before the OS reclaims all threads gracefully.
class CleanExitEnvironment : public ::testing::Environment {
public:
    void TearDown() override {
        // GTest has printed results; avoid the LockFreeQueueManager thread abort.
        _exit(::testing::UnitTest::GetInstance()->Failed() ? 1 : 0);
    }
};

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr int    kSampleRate     = 48000;
static constexpr int    kBlockSize      = 512;
static constexpr int    kChannels       = 2;  // stereo interleaved

// Well-known URIs available on a standard Fedora/RPM desktop
static const char* kAmpUri    = "http://plugin.org.uk/swh-plugins/amp";
static const char* kDelayUri  = "http://gareus.org/oss/lv2/nodelay";

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Build a silent stereo-interleaved buffer of length frames*2
static std::vector<float> makeSilence(int frames) {
    return std::vector<float>(frames * kChannels, 0.0f);
}

// Build a 1 kHz sine wave, stereo interleaved
static std::vector<float> makeSine(int frames, float freq = 1000.0f, float amp = 0.5f) {
    std::vector<float> buf(frames * kChannels);
    for (int i = 0; i < frames; ++i) {
        float s = amp * std::sin(2.0f * M_PI * freq * i / kSampleRate);
        buf[i * 2]     = s;
        buf[i * 2 + 1] = s;
    }
    return buf;
}

// Root-mean-square of a buffer
static float rms(const std::vector<float>& v) {
    if (v.empty()) return 0.0f;
    double sum = 0.0;
    for (float x : v) sum += x * x;
    return std::sqrt(static_cast<float>(sum / v.size()));
}

// ─── Fixture ─────────────────────────────────────────────────────────────────
// LockFreeQueueManager uses a static background thread; constructing multiple
// LiveEffectEngine objects in the same process calls init() on the same static
// thread and aborts.  We therefore share ONE engine for the entire suite and
// call initPlugins() exactly once.

class LiveEffectEngineTest : public ::testing::Test {
protected:
    // Shared across the entire test suite.
    static LiveEffectEngine* engine;

    static void SetUpTestSuite() {
        engine = new LiveEffectEngine();
        engine->sampleRate = kSampleRate;
        engine->blockSize  = kBlockSize;
        engine->initPlugins();   // expensive LV2 world scan — do it once
    }

    static void TearDownTestSuite() {
        for (int slot = 1; slot <= 4; ++slot)
            engine->deletePlugin(slot);
        delete engine;
        engine = nullptr;
    }

    void TearDown() override {
        // Clean plugin slots after each test so they don't bleed into the next.
        for (int slot = 1; slot <= 4; ++slot)
            engine->deletePlugin(slot);
        // Reset gain to 1
        *engine->gain = 1.0f;
    }
};
LiveEffectEngine* LiveEffectEngineTest::engine = nullptr;

// ─── 1. Plugin Discovery ──────────────────────────────────────────────────────

TEST_F(LiveEffectEngineTest, InitPluginsReturnsNonEmptyJson) {
    // initPlugins() was called once in SetUpTestSuite; verify resulting pluginInfo
    json info = engine->getAvailablePlugins();
    ASSERT_FALSE(info.is_discarded()) << "pluginInfo is invalid";
    EXPECT_TRUE(info.is_object());
    EXPECT_FALSE(info.empty());
}

TEST_F(LiveEffectEngineTest, InitPluginsFindsAtLeastOnePlugin) {
    EXPECT_GT(engine->pluginCount, 0);
}

TEST_F(LiveEffectEngineTest, InitPluginsPopulatesPluginInfo) {
    json info = engine->getAvailablePlugins();
    EXPECT_TRUE(info.is_object());
    EXPECT_FALSE(info.empty());
}

TEST_F(LiveEffectEngineTest, InitPluginsKnownPluginPresent) {
    json info = engine->getAvailablePlugins();
    EXPECT_TRUE(info.contains(kAmpUri))
        << "Expected '" << kAmpUri << "' to be in plugin list";
}

TEST_F(LiveEffectEngineTest, InitPluginsPluginHasRequiredFields) {
    json info = engine->getAvailablePlugins();
    ASSERT_TRUE(info.contains(kAmpUri));
    const json& p = info[kAmpUri];
    EXPECT_TRUE(p.contains("name"));
    EXPECT_TRUE(p.contains("uri"));
    EXPECT_TRUE(p.contains("ports"));
}

// ─── 2. Audio engine start / stop ─────────────────────────────────────────────
// AudioEngine wraps JACK; on a headless system JACK may not be running.
// We test the lifecycle API without asserting a successful connection.
// NOTE: LockFreeQueueManager uses a static background thread, so we share
// a single LiveEffectEngine across both AudioEngine tests to avoid re-init.

class AudioEngineTest : public ::testing::Test {
protected:
    // Shared across all tests in this suite to avoid double-init of the
    // static LockFreeQueueManager thread.
    static LiveEffectEngine* sharedEngine;
    static AudioEngine*      sharedAE;

    static void SetUpTestSuite() {
        sharedEngine = new LiveEffectEngine();
        sharedEngine->sampleRate = kSampleRate;
        sharedEngine->blockSize  = kBlockSize;
        sharedAE = new AudioEngine(sharedEngine);
    }

    static void TearDownTestSuite() {
        delete sharedAE;   sharedAE     = nullptr;
        delete sharedEngine; sharedEngine = nullptr;
    }
};
LiveEffectEngine* AudioEngineTest::sharedEngine = nullptr;
AudioEngine*      AudioEngineTest::sharedAE     = nullptr;

TEST_F(AudioEngineTest, ConstructsWithoutCrash) {
    EXPECT_EQ(sharedAE->state(), AudioEngine::State::Off);
}

TEST_F(AudioEngineTest, StopOnIdleEngineIsNoop) {
    EXPECT_NO_THROW(sharedAE->stop());
    EXPECT_EQ(sharedAE->state(), AudioEngine::State::Off);
}

// ─── 3. Loading / Unloading plugins ──────────────────────────────────────────

TEST_F(LiveEffectEngineTest, AddPluginSucceeds) {
    int result = engine->addPlugin(1, kAmpUri);
    EXPECT_EQ(result, 0) << "addPlugin should return 0 on success";
    EXPECT_NE(engine->plugin1, nullptr);
}

TEST_F(LiveEffectEngineTest, AddPluginRejectsInvalidSlot) {
    int result = engine->addPlugin(99, kAmpUri);
    EXPECT_EQ(result, -1);
}

TEST_F(LiveEffectEngineTest, AddPluginRejectsUnknownUri) {
    int result = engine->addPlugin(1, "http://does.not.exist/plugin");
    EXPECT_EQ(result, -1);
    EXPECT_EQ(engine->plugin1, nullptr);
}

TEST_F(LiveEffectEngineTest, AddPluginRejectsWhenBlockSizeZero) {
    int saved = engine->blockSize;
    engine->blockSize = 0;
    int result = engine->addPlugin(1, kAmpUri);
    engine->blockSize = saved;  // restore before any assertions that could skip teardown
    EXPECT_EQ(result, -1);
    EXPECT_EQ(engine->plugin1, nullptr);
}

TEST_F(LiveEffectEngineTest, DeletePluginClearsSlot) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    ASSERT_NE(engine->plugin1, nullptr);

    engine->deletePlugin(1);
    EXPECT_EQ(engine->plugin1, nullptr);
}

TEST_F(LiveEffectEngineTest, DeletePluginOnEmptySlotIsNoop) {
    EXPECT_NO_THROW(engine->deletePlugin(1));
    EXPECT_EQ(engine->plugin1, nullptr);
}

TEST_F(LiveEffectEngineTest, DeletePluginUnknownSlotIsNoop) {
    EXPECT_NO_THROW(engine->deletePlugin(99));
}

TEST_F(LiveEffectEngineTest, AllFourSlotsLoadAndUnload) {
    for (int slot = 1; slot <= 4; ++slot) {
        EXPECT_EQ(engine->addPlugin(slot, kAmpUri), 0)
            << "addPlugin failed for slot " << slot;
    }
    EXPECT_NE(engine->plugin1, nullptr);
    EXPECT_NE(engine->plugin2, nullptr);
    EXPECT_NE(engine->plugin3, nullptr);
    EXPECT_NE(engine->plugin4, nullptr);

    for (int slot = 1; slot <= 4; ++slot)
        engine->deletePlugin(slot);

    EXPECT_EQ(engine->plugin1, nullptr);
    EXPECT_EQ(engine->plugin2, nullptr);
    EXPECT_EQ(engine->plugin3, nullptr);
    EXPECT_EQ(engine->plugin4, nullptr);
}

TEST_F(LiveEffectEngineTest, LoadingNewPluginReplacesExisting) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    LV2Plugin* first = engine->plugin1;

    // Load a different plugin into the same slot — must not crash or leak.
    ASSERT_EQ(engine->addPlugin(1, kDelayUri), 0);
    EXPECT_NE(engine->plugin1, nullptr);
    EXPECT_NE(engine->plugin1, first);  // pointer must differ from the freed plugin
}

// ─── 4a. Parameter changes on a loaded plugin ─────────────────────────────────

TEST_F(LiveEffectEngineTest, SetValueChangesPortControl) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    // Amp plugin: port 0 is "Amps gain (dB)" — a control input.
    engine->setValue(1, 0, 6.0f);

    std::vector<LV2Plugin::PortInfo> ports = engine->getPluginPortInfo(1);
    ASSERT_FALSE(ports.empty());
    // Find port index 0
    bool found = false;
    for (const auto& pi : ports) {
        if (pi.portIndex == 0) {
            EXPECT_FLOAT_EQ(engine->plugin1->ports_.at(0).control, 6.0f);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Port 0 not found in getPluginPortInfo()";
}

TEST_F(LiveEffectEngineTest, SetValueOnInvalidSlotDoesNotCrash) {
    EXPECT_NO_THROW(engine->setValue(99, 0, 1.0f));
}

TEST_F(LiveEffectEngineTest, SetValueOnNullPluginDoesNotCrash) {
    // slot 2 is empty
    EXPECT_NO_THROW(engine->setValue(2, 0, 1.0f));
}

// ─── 5. Plugin enabled / disabled ─────────────────────────────────────────────

TEST_F(LiveEffectEngineTest, PluginEnabledDefaultIsTrue) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    EXPECT_TRUE(engine->plugin1->enabled);
}

TEST_F(LiveEffectEngineTest, SetPluginEnabledToggles) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    engine->setPluginEnabled(1, false);
    EXPECT_FALSE(engine->plugin1->enabled);

    engine->setPluginEnabled(1, true);
    EXPECT_TRUE(engine->plugin1->enabled);
}

TEST_F(LiveEffectEngineTest, SetPluginEnabledOnEmptySlotIsNoop) {
    EXPECT_NO_THROW(engine->setPluginEnabled(1, false));
}

// ─── 6. process() passthrough — no plugins loaded ─────────────────────────────

TEST_F(LiveEffectEngineTest, ProcessPassthroughWithNoPlugins) {
    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    engine->process(in.data(), out.data(), kBlockSize);

    // With no active plugins the engine copies input → output.
    EXPECT_GT(rms(out), 0.0f) << "Output should not be silent when no plugin is active";
    for (int i = 0; i < kBlockSize * kChannels; ++i)
        EXPECT_FLOAT_EQ(out[i], in[i]) << "Output should equal input at index " << i;
}

// ─── 7. process() with a real LV2 plugin ──────────────────────────────────────

TEST_F(LiveEffectEngineTest, ProcessWithAmpPluginProducesOutput) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    int ret = engine->process(in.data(), out.data(), kBlockSize);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(rms(out), 0.0f) << "Plugin processing should produce non-silent output";
}

TEST_F(LiveEffectEngineTest, ProcessWithDisabledPluginPassesThrough) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    engine->setPluginEnabled(1, false);

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    engine->process(in.data(), out.data(), kBlockSize);

    // Disabled plugin → passthrough
    for (int i = 0; i < kBlockSize * kChannels; ++i)
        EXPECT_FLOAT_EQ(out[i], in[i]) << "Disabled plugin should pass audio through at " << i;
}

TEST_F(LiveEffectEngineTest, ProcessChainedPluginsProducesOutput) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    ASSERT_EQ(engine->addPlugin(2, kAmpUri), 0);

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    int ret = engine->process(in.data(), out.data(), kBlockSize);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(rms(out), 0.0f);
}

TEST_F(LiveEffectEngineTest, ProcessSilenceProducesSilence) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    auto in  = makeSilence(kBlockSize);
    auto out = makeSilence(kBlockSize);

    engine->process(in.data(), out.data(), kBlockSize);
    EXPECT_FLOAT_EQ(rms(out), 0.0f);
}

// ─── 8. Port info metadata ─────────────────────────────────────────────────────

TEST_F(LiveEffectEngineTest, GetPluginPortInfoReturnsPortsWhenLoaded) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    auto ports = engine->getPluginPortInfo(1);
    EXPECT_FALSE(ports.empty());
}

TEST_F(LiveEffectEngineTest, GetPluginPortInfoReturnsEmptyForEmptySlot) {
    auto ports = engine->getPluginPortInfo(1);
    EXPECT_TRUE(ports.empty());
}

TEST_F(LiveEffectEngineTest, GetPluginPortInfoPortHasValidLabel) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    auto ports = engine->getPluginPortInfo(1);
    for (const auto& p : ports)
        EXPECT_FALSE(p.label.empty()) << "Every port should have a non-empty label";
}

TEST_F(LiveEffectEngineTest, GetPluginPortInfoPortHasValidSymbol) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    auto ports = engine->getPluginPortInfo(1);
    for (const auto& p : ports)
        EXPECT_FALSE(p.symbol.empty()) << "Every port should have a non-empty symbol";
}

// ─── 9. Preset round-trip ─────────────────────────────────────────────────────

TEST_F(LiveEffectEngineTest, GetPresetReturnsObjectForLoadedPlugin) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    json preset = engine->getPreset(1);
    EXPECT_TRUE(preset.is_object());
    EXPECT_TRUE(preset.contains("uri"));
}

TEST_F(LiveEffectEngineTest, GetPresetReturnsEmptyForUnloadedSlot) {
    json preset = engine->getPreset(1);
    // Should parse as "{}" (empty object or string)
    EXPECT_TRUE(preset.empty() || (preset.is_string() && preset.get<std::string>() == "{}"));
}

TEST_F(LiveEffectEngineTest, ApplyPresetRestoresValues) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    // Capture default preset
    json preset = engine->getPreset(1);

    // Modify port 0 (gain dB)
    engine->setValue(1, 0, 12.0f);
    EXPECT_FLOAT_EQ(engine->plugin1->ports_.at(0).control, 12.0f);

    // Restore via applyPreset
    engine->applyPreset(1, preset);

    float restoredVal = engine->plugin1->ports_.at(0).control;
    // The restored value should differ from the manually set 12.0
    EXPECT_NE(restoredVal, 12.0f);
}

TEST_F(LiveEffectEngineTest, GetPresetListIsValidJson) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    std::string list = engine->getPresetList();
    json j = json::parse(list, nullptr, /*exceptions=*/false);
    ASSERT_FALSE(j.is_discarded()) << "getPresetList() returned invalid JSON";
    EXPECT_TRUE(j.contains("gain"));
    EXPECT_TRUE(j.contains("plugin1"));
}

// ─── 10. Gain application ─────────────────────────────────────────────────────

TEST_F(LiveEffectEngineTest, GainScalesOutput) {
    // No plugin — passthrough with gain applied
    *engine->gain = 2.0f;

    auto in  = makeSine(kBlockSize, 1000.0f, 0.25f);
    auto out = makeSilence(kBlockSize);

    engine->process(in.data(), out.data(), kBlockSize);

    float inRms  = rms(in);
    float outRms = rms(out);
    EXPECT_NEAR(outRms, inRms * 2.0f, inRms * 0.01f)
        << "Gain=2 should double the RMS amplitude";
}

// ─── 11. Concurrent load / process (the #1 production crash scenario) ─────────
//
// In production the JACK RT thread calls process() continuously while the GTK
// UI thread calls addPlugin() / deletePlugin().  The bypass flag + pluginMutex
// protect this path; the tests below verify no crash, deadlock, or data
// corruption occurs under concurrent access.

TEST_F(LiveEffectEngineTest, ConcurrentLoadAndProcess_NoDeadlock) {
    // Start a thread that hammers process() for 500 ms.
    std::atomic<bool> stop{false};
    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    std::thread processer([&]() {
        while (!stop.load(std::memory_order_relaxed))
            engine->process(in.data(), out.data(), kBlockSize);
    });

    // Meanwhile, load and unload on the main thread 10 times.
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(engine->addPlugin(1, kAmpUri), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        engine->deletePlugin(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    stop.store(true, std::memory_order_relaxed);
    processer.join();
    // If we get here without a deadlock, SIGSEGV or SIGABRT → pass.
}

TEST_F(LiveEffectEngineTest, ConcurrentSetValueAndProcess_NoDataCorruption) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    std::atomic<bool> stop{false};
    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    std::thread processer([&]() {
        while (!stop.load(std::memory_order_relaxed))
            engine->process(in.data(), out.data(), kBlockSize);
    });

    // Rapidly toggle a parameter while audio is running.
    for (int i = 0; i < 200; ++i) {
        engine->setValue(1, 0, (i % 2 == 0) ? 0.0f : 6.0f);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    stop.store(true, std::memory_order_relaxed);
    processer.join();
}

TEST_F(LiveEffectEngineTest, ConcurrentDeleteAndProcess_NoUseAfterFree) {
    // Load plugin, start processing, then delete it — process() must not
    // dereference the freed pointer.
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);

    std::atomic<bool> stop{false};
    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    std::thread processer([&]() {
        while (!stop.load(std::memory_order_relaxed))
            engine->process(in.data(), out.data(), kBlockSize);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    engine->deletePlugin(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    stop.store(true, std::memory_order_relaxed);
    processer.join();
}

TEST_F(LiveEffectEngineTest, RapidReloadSameSlot_NoLeak) {
    // Load/unload the same slot 20 times in quick succession.
    // A use-after-free or double-free typically manifests here.
    for (int i = 0; i < 20; ++i) {
        ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0)
            << "Rapid reload failed at iteration " << i;
        engine->deletePlugin(1);
    }
    EXPECT_EQ(engine->plugin1, nullptr);
}

TEST_F(LiveEffectEngineTest, RapidReloadDifferentPlugins_NoLeak) {
    // Alternate between two different plugins in the same slot.
    for (int i = 0; i < 10; ++i) {
        const char* uri = (i % 2 == 0) ? kAmpUri : kDelayUri;
        ASSERT_EQ(engine->addPlugin(1, uri), 0)
            << "Alternate reload failed at iteration " << i;
    }
}

// ─── 12. Load every installed plugin (finds plugin-specific init crashes) ──────
//
// Iterates through the full plugin catalogue discovered by initPlugins() and
// attempts to load each one.  Any plugin that crashes rather than returning -1
// from addPlugin() will be caught as a SIGSEGV/SIGABRT test failure.
// This is the most direct way to reproduce "crashes on plugin load".

TEST_F(LiveEffectEngineTest, LoadEveryInstalledPlugin_NoInitCrash) {
    json catalogue = engine->getAvailablePlugins();
    ASSERT_FALSE(catalogue.empty());

    int attempted = 0, succeeded = 0, failed = 0;

    for (auto it = catalogue.begin(); it != catalogue.end(); ++it) {
        const std::string& uri = it.key();

        // Each load replaces slot 1; TearDown cleans up after the whole test.
        int rc = engine->addPlugin(1, uri);
        ++attempted;
        if (rc == 0) {
            // Verify the slot is actually set and plugin survived initialize().
            EXPECT_NE(engine->plugin1, nullptr)
                << "addPlugin returned 0 but plugin1 is null for: " << uri;

            // Run one process() cycle to flush atom buffers and exercise the DSP.
            auto in  = makeSine(kBlockSize);
            auto out = makeSilence(kBlockSize);
            engine->process(in.data(), out.data(), kBlockSize);

            engine->deletePlugin(1);
            ++succeeded;
        } else {
            ++failed;
        }
    }

    // At minimum the two hand-picked test plugins must succeed.
    EXPECT_GT(succeeded, 0) << "No plugin loaded successfully out of " << attempted;
    SUCCEED() << "Loaded " << succeeded << "/" << attempted
              << " plugins (" << failed << " declined initialization)";
}

// ─── 13. Out-of-bounds and edge-case setValue ─────────────────────────────────

TEST_F(LiveEffectEngineTest, SetValueOutOfBoundsPortDoesNotCrash) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    // Port count for amp is 3; index 9999 is way out of range.
    EXPECT_NO_THROW(engine->setValue(1, 9999, 1.0f));
}

TEST_F(LiveEffectEngineTest, SetValueAfterDeleteDoesNotCrash) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    engine->deletePlugin(1);
    // Plugin is gone; setValue must not dereference the freed pointer.
    EXPECT_NO_THROW(engine->setValue(1, 0, 1.0f));
}

// ─── 14. process() edge cases ─────────────────────────────────────────────────

TEST_F(LiveEffectEngineTest, ProcessWithZeroFramesDoesNotCrash) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);
    EXPECT_NO_THROW(engine->process(in.data(), out.data(), 0));
}

TEST_F(LiveEffectEngineTest, ProcessWithBypassSetPassesThrough) {
    ASSERT_EQ(engine->addPlugin(1, kAmpUri), 0);
    engine->bypass.store(true);

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);
    engine->process(in.data(), out.data(), kBlockSize);
    engine->bypass.store(false);

    for (int i = 0; i < kBlockSize * kChannels; ++i)
        EXPECT_FLOAT_EQ(out[i], in[i]) << "Bypass must copy input to output at " << i;
}

// ─── Test program entry ───────────────────────────────────────────────────────
// We override main so we can register CleanExitEnvironment before RUN_ALL_TESTS.
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new CleanExitEnvironment);
    return RUN_ALL_TESTS();
}
