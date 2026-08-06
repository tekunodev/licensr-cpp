# licensr_juce

Optional JUCE module wrapping the [C++ SDK](../README.md) core in a drop-in `LicenseActivationComponent` — a self-contained `juce::Component` with a license-key field, an activate button, and status label, for plugins that want a working activation UI without hand-rolling one.

## Why this is a separate module

JUCE itself ships under AGPLv3-or-commercial terms. Folding a JUCE dependency into the core `licensr` target would force that licensing decision onto every consumer of the SDK, including the many plugin hosts, game engines, and desktop apps that have nothing to do with JUCE. `licensr_juce` is MIT-licensed like the core, but only ever gets compiled into projects that have already made their own JUCE licensing decision — it adds nothing new to that decision.

## Install

Add this subdirectory after your project has already loaded JUCE's CMake support and the `licensr` core target:

```cmake
add_subdirectory(path/to/JUCE)
add_subdirectory(path/to/licensr/sdks/cpp)
add_subdirectory(path/to/licensr/sdks/cpp/juce)
target_link_libraries(YourPluginTarget PRIVATE licensr_juce)
```

`licensr_juce` is an `INTERFACE` dependency on `licensr::licensr` (the core SDK) — linking it pulls in the full `licensr::` API alongside the component.

## Quickstart

```cpp
#include <licensr_juce/licensr_juce.h>

licensr::ClientConfig config;
config.api_key = "pk_live_...";
config.plugin_slug = "my-plugin";

auto activationComponent = std::make_unique<licensr::LicenseActivationComponent>(
    config, licensr::compute_hardware_id(config.plugin_slug).value_or(fallback_uuid));

activationComponent->onActivated = [this](const licensr::ActivateResult& result) {
  // unlock the editor, close the activation view, etc.
};
activationComponent->onError = [](const licensr::Error& error) {
  // error.message is already user-presentable
};

addAndMakeVisible(*activationComponent);
```

Every `licensr::Client` callback fires on Licensr's background worker thread (see the core SDK's README); `LicenseActivationComponent` always hops back to the JUCE message thread via `juce::MessageManager::callAsync` — guarded by a `juce::Component::SafePointer` in case the component is destroyed mid-request — before touching UI state or invoking `onActivated`/`onError`. Those two callbacks are the only two touched from the message thread; everything else on `licensr::Client` keeps the core SDK's own threading rules.

`LicenseActivationComponent` owns its own `Client` (and that `Client`'s share of the background worker thread) for its lifetime — construct one per plugin/editor instance, not per window if you have several editor windows open at once. Reach the rest of the license API surface (checkout, deactivate, activations) via `component.client()`.

## Development

This module has no unit tests of its own — it was validated with a throwaway console app linking `licensr_juce` against a real JUCE checkout, run under `juce::ScopedJuceInitialiser_GUI`, confirming the module compiles, links, and instantiates `LicenseActivationComponent` without crashing. The C++ CI matrix does not currently build this module since JUCE is a multi-hundred-megabyte dependency; validate manually against your own JUCE checkout when touching it.
