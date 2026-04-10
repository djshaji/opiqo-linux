// tests/test_lv2plugin.cpp
//
// Unit tests for LV2Plugin loading and unloading lifecycle.
//
// Test coverage:
//   1.  Load a valid plugin URI → initialize() succeeds
//   2.  Load an invalid/unknown URI → initialize() fails gracefully
//   3.  Initialize with null world → initialize() fails gracefully
//   4.  start() / stop() lifecycle — no crash
//   5.  process() after start() returns true and produces output
//   6.  process() on a stopped plugin returns false
//   7.  process() on a disabled (bypass) plugin copies input → output
//   8.  process() with null buffers does not crash
//   9.  process() with zero frames does not crash
//  10.  closePlugin() is idempotent — calling it twice does not crash
//  11.  closePlugin() followed by process() does not crash (use-after-close guard)
//  12.  Sequential load → close → reload using the same Lilv world
//  13.  getControlPortInfo() returns valid metadata after initialize()
//  14.  getControlPortInfo() returns empty before initialize()
//  15.  Load every installed plugin in a fork-isolated child — catalogues crashers
//  16.  Rapid load/close loop — no leak or crash
//
// Requires at least one of:
//   - http://plugin.org.uk/swh-plugins/amp          (SWH amp, /usr/lib64/lv2)
//   - http://plugin.org.uk/swh-plugins/tapeDelay    (SWH tape delay)
// Both are part of the swh-plugins-lv2 package on Fedora/RPM systems.

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "logging_macros.h"
#include "LV2Plugin.hpp"

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr double   kSampleRate  = 48000.0;
static constexpr uint32_t kBlockSize   = 512;
static constexpr int      kChannels    = 2;

// Well-known URIs expected on a standard Fedora/RPM desktop (swh-plugins-lv2)
static const char* kAmpUri       = "http://plugin.org.uk/swh-plugins/amp";
static const char* kTapeDelayUri = "http://plugin.org.uk/swh-plugins/tapeDelay";
static const char* kBadUri       = "http://example.invalid/no-such-plugin";

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::vector<float> makeSilence(int frames) {
    return std::vector<float>(frames * kChannels, 0.0f);
}

static std::vector<float> makeSine(int frames, float freq = 440.0f, float amp = 0.5f) {
    std::vector<float> buf(frames * kChannels);
    for (int i = 0; i < frames; ++i) {
        float s = amp * std::sin(2.0f * M_PI * freq * i / kSampleRate);
        buf[i * 2]     = s;
        buf[i * 2 + 1] = s;
    }
    return buf;
}

static float rms(const std::vector<float>& v) {
    if (v.empty()) return 0.0f;
    double sum = 0.0;
    for (float x : v) sum += static_cast<double>(x) * x;
    return std::sqrt(static_cast<float>(sum / v.size()));
}

// ─── Fixture ─────────────────────────────────────────────────────────────────
// One LilvWorld is created per test suite (expensive LV2 scan).
// Each test creates its own LV2Plugin instances that share the world.
// closePlugin() + explicit delete clean up between tests.

class LV2PluginTest : public ::testing::Test {
protected:
    static LilvWorld* world_;

    static void SetUpTestSuite() {
        world_ = lilv_world_new();
        lilv_world_load_all(world_);
    }

    static void TearDownTestSuite() {
        if (world_) {
            lilv_world_free(world_);
            world_ = nullptr;
        }
    }

    // Convenience: build and initialize a plugin; returns nullptr on failure.
    static LV2Plugin* makePlugin(const char* uri) {
        auto* p = new LV2Plugin(world_, uri, kSampleRate, kBlockSize);
        if (!p->initialize()) {
            delete p;
            return nullptr;
        }
        return p;
    }
};

LilvWorld* LV2PluginTest::world_ = nullptr;

// ─── 1. Load a valid plugin URI ───────────────────────────────────────────────

TEST_F(LV2PluginTest, InitializeWithValidUriSucceeds) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr) << "initialize() must succeed for a known installed plugin";
    p->closePlugin();
    delete p;
}

// ─── 2. Load an invalid/unknown URI ───────────────────────────────────────────

TEST_F(LV2PluginTest, InitializeWithUnknownUriReturnsFalse) {
    LV2Plugin plugin(world_, kBadUri, kSampleRate, kBlockSize);
    EXPECT_FALSE(plugin.initialize())
        << "initialize() must return false when the plugin URI is not installed";
}

// ─── 3. Initialize with null world ────────────────────────────────────────────

TEST_F(LV2PluginTest, InitializeWithNullWorldReturnsFalse) {
    LV2Plugin plugin(nullptr, kAmpUri, kSampleRate, kBlockSize);
    EXPECT_FALSE(plugin.initialize())
        << "initialize() must return false when LilvWorld is null";
}

// ─── 4. start() / stop() lifecycle ────────────────────────────────────────────

TEST_F(LV2PluginTest, StartStopDoesNotCrash) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);

    EXPECT_NO_THROW(p->start());
    EXPECT_NO_THROW(p->stop());

    p->closePlugin();
    delete p;
}

TEST_F(LV2PluginTest, MultipleStartStopCyclesDoNotCrash) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);

    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(p->start());
        EXPECT_NO_THROW(p->stop());
    }

    p->closePlugin();
    delete p;
}

// ─── 5. process() after start() ───────────────────────────────────────────────

TEST_F(LV2PluginTest, ProcessAfterStartReturnsTrueAndProducesAudio) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    EXPECT_TRUE(p->process(in.data(), out.data(), kBlockSize));
    EXPECT_GT(rms(out), 0.0f) << "Amp plugin should produce non-silent output";

    p->stop();
    p->closePlugin();
    delete p;
}

// ─── 6. process() on a stopped plugin ─────────────────────────────────────────

TEST_F(LV2PluginTest, ProcessAfterStopReturnsFalse) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();
    p->stop();

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    EXPECT_FALSE(p->process(in.data(), out.data(), kBlockSize))
        << "process() must return false after stop() sets the shutdown flag";

    p->closePlugin();
    delete p;
}

// ─── 7. process() with plugin disabled (bypass) ───────────────────────────────

TEST_F(LV2PluginTest, ProcessWithDisabledPluginCopiesInputToOutput) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();
    p->enabled = false;

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);

    EXPECT_TRUE(p->process(in.data(), out.data(), kBlockSize));

    for (uint32_t i = 0; i < kBlockSize * kChannels; ++i)
        EXPECT_FLOAT_EQ(out[i], in[i])
            << "Disabled plugin must copy input to output at index " << i;

    p->stop();
    p->closePlugin();
    delete p;
}

// ─── 8. process() with null buffers ───────────────────────────────────────────

TEST_F(LV2PluginTest, ProcessWithNullInputDoesNotCrash) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();

    auto out = makeSilence(kBlockSize);
    EXPECT_NO_THROW(p->process(nullptr, out.data(), kBlockSize));

    p->stop();
    p->closePlugin();
    delete p;
}

TEST_F(LV2PluginTest, ProcessWithNullOutputDoesNotCrash) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();

    auto in = makeSine(kBlockSize);
    EXPECT_NO_THROW(p->process(in.data(), nullptr, kBlockSize));

    p->stop();
    p->closePlugin();
    delete p;
}

// ─── 9. process() with zero frames ────────────────────────────────────────────

TEST_F(LV2PluginTest, ProcessWithZeroFramesDoesNotCrash) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);
    EXPECT_NO_THROW(p->process(in.data(), out.data(), 0));

    p->stop();
    p->closePlugin();
    delete p;
}

// ─── 10. closePlugin() is idempotent ──────────────────────────────────────────

TEST_F(LV2PluginTest, ClosePluginIsIdempotent) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();
    p->stop();

    EXPECT_NO_THROW(p->closePlugin());
    EXPECT_NO_THROW(p->closePlugin()); // second call must be a no-op
    delete p;
}

// ─── 11. process() after closePlugin() does not crash ─────────────────────────

TEST_F(LV2PluginTest, ProcessAfterCloseDoesNotCrash) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);
    p->start();
    p->closePlugin();

    auto in  = makeSine(kBlockSize);
    auto out = makeSilence(kBlockSize);
    EXPECT_NO_THROW(p->process(in.data(), out.data(), kBlockSize));
    delete p;
}

// ─── 12. Sequential load → close → reload with the same world ─────────────────

TEST_F(LV2PluginTest, SequentialLoadCloseReloadSameUri) {
    for (int i = 0; i < 3; ++i) {
        LV2Plugin* p = makePlugin(kAmpUri);
        ASSERT_NE(p, nullptr) << "Reload #" << i << " failed";
        p->start();

        auto in  = makeSine(kBlockSize);
        auto out = makeSilence(kBlockSize);
        EXPECT_TRUE(p->process(in.data(), out.data(), kBlockSize));

        p->stop();
        p->closePlugin();
        delete p;
    }
}

TEST_F(LV2PluginTest, SequentialLoadCloseDifferentPlugins) {
    const char* uris[] = { kAmpUri, kTapeDelayUri, kAmpUri };
    for (const char* uri : uris) {
        LV2Plugin* p = new LV2Plugin(world_, uri, kSampleRate, kBlockSize);
        if (!p->initialize()) {
            // Plugin may not be installed — skip gracefully
            delete p;
            continue;
        }
        p->start();
        p->stop();
        p->closePlugin();
        delete p;
    }
}

// ─── 13. getControlPortInfo() after initialize() ─────────────────────────────

TEST_F(LV2PluginTest, GetControlPortInfoReturnsPortsAfterInit) {
    LV2Plugin* p = makePlugin(kAmpUri);
    ASSERT_NE(p, nullptr);

    auto ports = p->getControlPortInfo();
    EXPECT_FALSE(ports.empty()) << "Amp plugin must expose at least one control port";

    for (const auto& info : ports) {
        EXPECT_FALSE(info.symbol.empty()) << "Every port must have a non-empty symbol";
        EXPECT_FALSE(info.label.empty())  << "Every port must have a non-empty label";
        EXPECT_LE(info.minVal, info.maxVal) << "minVal must be <= maxVal for port " << info.symbol;
    }

    p->closePlugin();
    delete p;
}

// ─── 14. getControlPortInfo() before initialize() ────────────────────────────

TEST_F(LV2PluginTest, GetControlPortInfoBeforeInitReturnsEmpty) {
    LV2Plugin plugin(world_, kAmpUri, kSampleRate, kBlockSize);
    // Not initialized — ports_ is empty, plugin_ not yet resolved to ports
    auto ports = plugin.getControlPortInfo();
    EXPECT_TRUE(ports.empty())
        << "getControlPortInfo() before initialize() must return an empty list";
}

// ─── 15. Load every installed plugin (fork-isolated) ─────────────────────────
//
// Each plugin is loaded, processed, and closed in a forked child process.
// A child that crashes (SIGSEGV, SIGABRT, etc.) does not kill the test runner.
// Three outcome codes are used:
//   _exit(0)  — plugin loaded, processed, and closed successfully
//   _exit(1)  — plugin gracefully declined initialize()
//   crash     — child killed by signal (catalogued as a crasher)
//
// Known crashers on a headless system:
//   - http://drumkv1.sourceforge.net/lv2
//       Calls QApplication inside lv2_instantiate(); Qt aborts without a display.
//       This is a plugin bug (GUI state in the DSP path) and not fixable in the host.
//       It will work correctly in a real desktop session.

TEST_F(LV2PluginTest, LoadEveryInstalledPlugin_NoInitOrProcessCrash) {
    const LilvPlugins* all = lilv_world_get_all_plugins(world_);
    ASSERT_NE(all, nullptr);

    int attempted = 0, succeeded = 0, declined = 0;
    std::vector<std::string> crashed_uris;

    LILV_FOREACH(plugins, it, all) {
        const LilvPlugin* lp       = lilv_plugins_get(all, it);
        const LilvNode*   uri_node = lilv_plugin_get_uri(lp);
        const std::string uri      = lilv_node_as_uri(uri_node);
        ++attempted;

        pid_t pid = fork();
        if (pid == 0) {
            // ── Child process ──────────────────────────────────────────────
            // Redirect stdout/stderr to silence plugin log noise.
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }

            LV2Plugin* p = new LV2Plugin(world_, uri.c_str(), kSampleRate, kBlockSize);
            if (!p->initialize()) {
                delete p;
                _exit(1); // graceful decline
            }
            p->start();
            std::vector<float> in  = makeSine(kBlockSize);
            std::vector<float> out = makeSilence(kBlockSize);
            p->process(in.data(), out.data(), kBlockSize);
            p->stop();
            p->closePlugin();
            delete p;
            _exit(0); // success
        }

        // ── Parent process ────────────────────────────────────────────────
        ASSERT_GT(pid, 0) << "fork() failed for plugin: " << uri;
        int status = 0;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0)
                ++succeeded;
            else
                ++declined;
        } else if (WIFSIGNALED(status)) {
            // Child was killed by a signal (SIGSEGV, SIGABRT, etc.)
            std::fprintf(stdout, "[CRASH   ] signal %d: %s\n", WTERMSIG(status), uri.c_str());
            std::fflush(stdout);
            crashed_uris.push_back(uri);
        }
    }

    // Report results.
    std::fprintf(stdout,
        "\n=== Catalogue results: %d succeeded, %d declined init, %d crashed / %d total ===\n",
        succeeded, declined, (int)crashed_uris.size(), attempted);
    for (const auto& u : crashed_uris)
        std::fprintf(stdout, "  CRASH: %s\n", u.c_str());
    std::fflush(stdout);

    EXPECT_EQ(crashed_uris.size(), 0u)
        << crashed_uris.size() << " plugin(s) crashed — see [CRASH] lines above.";
}

// ─── 16. Rapid load/close loop ────────────────────────────────────────────────

TEST_F(LV2PluginTest, RapidLoadCloseLoop_NoLeakOrCrash) {
    for (int i = 0; i < 20; ++i) {
        LV2Plugin* p = makePlugin(kAmpUri);
        ASSERT_NE(p, nullptr) << "Rapid loop failed at iteration " << i;
        p->start();
        p->stop();
        p->closePlugin();
        delete p;
    }
}
