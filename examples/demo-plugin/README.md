# Licensr Demo Synth

A real, open-source VST3/AU/Standalone synth that exists to exercise the [C++ SDK](../../README.md) end to end — this is the flagship integration test and launch asset for `sdks/cpp`, in the spirit of LicenseSeat's SynthDemo. It's a deliberately minimal 8-voice sine synth; the interesting part is the license activation flow wired around it.

## What it demonstrates

- `licensr::compute_hardware_id` for a stable per-machine identifier, reused as both the `device_id` (rate-limit bucketing) and the `activate()` seat identifier.
- `licensr::Client::activate` to register a seat, then `licensr::Client::validate` to populate `client().snapshot()`.
- `client().snapshot()` read directly from `processBlock` — the audio thread — to gate playback without ever blocking on network I/O.
- A persisted license key (via `juce::PropertiesFile`, the demo's own settings file) re-validated on every launch, per the pattern described in the core SDK README's [Token storage](../../README.md#token-storage) section.

## The demo limitation

Unlicensed, playback mutes for `LicensrDemoSynthAudioProcessor::kMuteSeconds` (5s) every `kAudibleSeconds` (10s), looping — a classic, transparent trial limitation. Activate a real license and it plays indefinitely. This is implemented entirely in `processBlock` by reading `isFullyLicensed()`; there's no separate "unlock" code path to keep in sync.

## Build

JUCE is fetched automatically via `FetchContent` (pinned to `8.0.6`), so this builds standalone without any manual JUCE checkout:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target LicensrDemoSynth_Standalone   # or _VST3 / _AU
```

If you're already vendoring JUCE elsewhere (embedding this example in a larger project), configure with `-DLICENSR_DEMO_USE_FETCHCONTENT=OFF` and `add_subdirectory(path/to/JUCE)` yourself before this directory.

Run the Standalone build directly — no DAW required — for the fastest way to try the activation flow:

```bash
open "build/LicensrDemoSynth_artefacts/Debug/Standalone/Licensr Demo Synth.app"          # macOS
./build/LicensrDemoSynth_artefacts/Debug/Standalone/LicensrDemoSynth                      # Windows/Linux
```

## Pointing it at a real Licensr account

The built-in defaults (`pk_test_REPLACE_ME` / `licensr-demo-synth` / `https://api.licensr.app`) don't correspond to a real plugin, so an unconfigured build only ever exercises the network/API-error path. Override at runtime with environment variables to test against your own account, or a local/staging backend:

```bash
LICENSR_DEMO_API_KEY=pk_test_... \
LICENSR_DEMO_PLUGIN_SLUG=my-test-plugin \
LICENSR_DEMO_BASE_URL=http://localhost:8000 \
  ./build/LicensrDemoSynth_artefacts/Debug/Standalone/LicensrDemoSynth
```

## Why the editor doesn't use `licensr_juce`

The optional [`licensr_juce`](../../juce/README.md) module's `LicenseActivationComponent` is a great drop-in for hosts that don't need to customize the flow. This demo's editor needs to persist the entered license key itself (to re-validate it on the next launch), which that component doesn't expose — so it's hand-rolled directly on `licensr::Client` instead. See `Source/PluginEditor.h` for the full rationale.
