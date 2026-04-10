# Opiqo Unit Tests

Test suite: `tests/test_live_effect_engine.cpp`  
Build target: `opiqo_tests`  
Framework: Google Test

## Building

```bash
cd build-linux-debug
cmake .. -DOPIQO_TARGET_PLATFORM=linux
make opiqo_tests
```

## Running

```bash
# All tests
./opiqo_tests

# Specific test or group
./opiqo_tests --gtest_filter="LiveEffectEngineTest.*"
./opiqo_tests --gtest_filter="*Concurrent*"
```

## Coverage

### 1. Plugin Discovery
Verifies that `initPlugins()` scans the LV2 path, populates `pluginInfo`, and returns valid JSON with required fields (`name`, `uri`, `ports`).

### 2. Audio Engine Lifecycle
Constructs `AudioEngine`, verifies initial state is `Off`, and confirms that calling `stop()` on an idle engine is a no-op. Uses a JACK stub so these tests run without a live JACK server.

### 3. Loading / Unloading Plugins
- Successful load into all four slots
- Rejection of invalid slot numbers
- Rejection of unknown plugin URIs
- Rejection when `blockSize == 0` (engine not started)
- `deletePlugin()` on an occupied, empty, or invalid slot
- Loading a new plugin into an occupied slot replaces the old one

### 4. Parameter Changes
- `setValue()` updates `port.control` on the correct port
- Safe handling of invalid slot, null plugin, and out-of-bounds port index
- `setValue()` called after `deletePlugin()` does not crash

### 5. Plugin Enable / Disable
Toggles `plugin->enabled` via `setPluginEnabled()` and verifies the flag; confirms no-op on an empty slot.

### 6. `process()` — No Plugins
With all slots empty, `process()` copies input to output unchanged.

### 7. `process()` — With Plugins
- Active plugin produces non-silent output from a sine input
- Disabled plugin passes audio through unchanged
- Two chained plugins both process correctly
- Silent input produces silent output (no DC injection)
- `bypass` flag forces passthrough regardless of loaded plugins
- Zero-frame call does not crash

### 8. Port Metadata
`getPluginPortInfo()` returns non-empty results for a loaded plugin, empty for an unloaded slot, and every port has a non-empty `label` and `symbol`.

### 9. Preset Round-Trip
- `getPreset()` returns a JSON object containing `uri` for a loaded plugin
- `getPreset()` on an empty slot returns an empty/null value
- `applyPreset()` restores port values changed by `setValue()`
- `getPresetList()` returns valid JSON with `gain` and `plugin1`–`plugin4` keys

### 10. Gain
With `*gain = 2.0`, output RMS is double the input RMS (±1%).

### 11. Concurrent Access (Production Crash Scenarios)

These tests directly target the race conditions most likely to cause production crashes.

| Test | What it catches |
|---|---|
| `ConcurrentLoadAndProcess_NoDeadlock` | UI calls `addPlugin()` while the RT thread is in `process()` — the primary real-world race |
| `ConcurrentSetValueAndProcess_NoDataCorruption` | Parameter update races with `process()` reading the same `port.control` float |
| `ConcurrentDeleteAndProcess_NoUseAfterFree` | RT thread dereferences plugin pointer after UI thread freed it via `deletePlugin()` |
| `RapidReloadSameSlot_NoLeak` | Same slot reloaded 20 times — catches double-free or use-after-free in `closePlugin()` |
| `RapidReloadDifferentPlugins_NoLeak` | Alternating between two plugins — exposes teardown bugs |

### 12. Load Every Installed Plugin

`LoadEveryInstalledPlugin_NoInitCrash` iterates every plugin returned by `initPlugins()`, calls `addPlugin()`, runs one `process()` cycle, and calls `deletePlugin()`. This directly reproduces crashes caused by specific plugin `initialize()` calls and is the most reliable way to find plugin-specific bugs.

### 13. Edge Cases
- `setValue()` with an out-of-bounds port index
- `setValue()` after the plugin has been deleted
- `process()` with zero frames

---

## Reproducing Production Crashes

Run the full-catalogue test to get stack traces when a specific plugin crashes:

```bash
cd build-linux-debug
cmake .. -DOPIQO_TARGET_PLATFORM=linux
make opiqo_tests
./opiqo_tests --gtest_filter="*LoadEveryInstalled*"
```

To run only the concurrency tests:

```bash
cmake .. -DOPIQO_TARGET_PLATFORM=linux
make opiqo_tests
./opiqo_tests --gtest_filter="*Concurrent*:*Rapid*"
```

## Known Limitations

- `AudioEngine` tests use a stub and do not exercise the real JACK callback path. JACK concurrency is implicitly tested via the `Concurrent*` tests which simulate the same interleave pattern manually.
- `LockFreeQueueManager` spawns a static background thread that cannot be cleanly joined (the `quit()` join is commented out in the source). The test binary uses `_exit()` after all tests complete to avoid an abort on teardown. This is a known issue in the production code, not the test harness.
