# licensr (C++ SDK)

Official C++ SDK for the [Licensr](https://licensr.app) license API, built for audio plugins and other native, closed-source hosts. Zero third-party dependencies: no OpenSSL, no nlohmann/json, no networking library — everything the SDK needs (JSON, HTTP, and the Ed25519 primitives that the upcoming offline-verify module will use) is either vendored and trimmed to the minimum or backed by the OS's own networking stack.

See the [License calls guide](https://docs.licensr.app/plugin-integration) for the underlying API's concepts (activation modes, entitlement/fallback, rate limits, the embedded-key doctrine) — this README covers the C++-specific surface.

## Why this SDK looks the way it does

A DAW loads plugins in-process, and two different plugins may both statically link Licensr into the same host. That constrains the design in ways a typical SDK doesn't have to worry about:

- **Hidden symbol visibility.** Every exported symbol lives under `licensr::` with `-fvisibility=hidden` by default (see `include/licensr/export.h`) so two plugins' copies of Licensr never collide or accidentally share state through the dynamic symbol table.
- **No global mutable state.** The only process-wide thing is a refcounted registry of `ClientCore` instances (see `src/detail/singleton.h`), keyed by installation (`api_key` + `plugin_slug` + `base_url`) — so N instances of the same plugin in one DAW session share one worker thread and one set of in-flight requests instead of firing N heartbeats.
- **Nothing blocks the audio thread, ever.** `Client::validate/activate/deactivate/activations/token/checkout` are asynchronous: they enqueue work onto a single background worker thread and invoke your callback from that thread. `Client::snapshot()` is the audio-thread-safe read path — an atomically-swapped, immutable `ValidateResult` that never allocates and never blocks (`std::atomic_load` on a `shared_ptr` is just a refcount bump).

## Install

CMake `FetchContent` against the public mirror. Pin `GIT_TAG` to a release from
[tekunodev/licensr-cpp](https://github.com/tekunodev/licensr-cpp/releases) (not `main`).
If no tag is published yet, vendor `sdks/cpp/` and `add_subdirectory(...)`.

```cmake
include(FetchContent)
FetchContent_Declare(licensr
    GIT_REPOSITORY https://github.com/tekunodev/licensr-cpp.git
    GIT_TAG v0.2.1
)
FetchContent_MakeAvailable(licensr)
target_link_libraries(your_plugin PRIVATE licensr::licensr)
```

## Quickstart

```cpp
#include <licensr/licensr.h>

licensr::ClientConfig config;
config.api_key = "pk_live_...";  // safe to embed in a distributed binary — see the License calls guide
config.plugin_slug = "my-plugin";

licensr::Client client(config);
client.validate(user_entered_key, [](licensr::Result<licensr::ValidateResult> result) {
  if (!result.ok()) {
    // result.error().kind is kApi (branch on .code) or kNetwork.
    return;
  }
  if (result.value().valid) {
    // unlock full features
  } else if (result.value().entitlement == licensr::Entitlement::kLimited) {
    // perpetual-fallback: expired, but the plan grants a limited tier
  }
});

// Elsewhere, e.g. once per processBlock — never blocks, never allocates:
if (auto snapshot = client.snapshot()) {
  bool valid = snapshot->valid;
}
```

The result keeps what you act on at the top level: `valid`, `status` (why, for your message), `entitlement` and `feature_flags`. Plan and limit details are grouped under `result.license` (`plan_id`, `activation_mode`, `max_seats`, `max_domains`, `seats_in_use`, `expires_at`). They are for display only; never use them to decide whether to unlock.

## API

Every method is asynchronous and calls `callback` from Licensr's worker thread with a `Result<T>` — never both a value and an error, and never thrown.

| Method                                                  | Wraps                         | Notes                                                                                                                                                                                              |
| ------------------------------------------------------- | ----------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `client.validate(licenseKey, callback)`                 | `POST /v1/license/validate`   | Always read `valid`/`entitlement`; never branch on HTTP status — an unknown key returns `200 {valid: false}`.                                                                                      |
| `client.token(licenseKey, callback)`                    | `POST /v1/license/token`      | Same check as `validate`, plus a short-lived signed offline token. Offline verification lands in a follow-up SDK module — for now, verify server-side or via the JS/TS SDK's `verifyOfflineToken`. |
| `client.activate(request, callback)`                    | `POST /v1/license/activate`   | `activation_type` is `kSeat` (per-machine) or `kDomain`, fixed by the plugin's configured mode.                                                                                                    |
| `client.deactivate(licenseKey, activationId, callback)` | `POST /v1/license/deactivate` | Frees a seat/domain slot.                                                                                                                                                                          |
| `client.activations(licenseKey, callback)`              | `GET /v1/license/activations` | Lists every current activation.                                                                                                                                                                    |
| `client.checkout(request, callback)`                    | `POST /v1/billing/checkout`   | Returns `checkout_url` — direct the user there. Requires a key with the `checkout` scope.                                                                                                          |

With a test key (`pk_test_…`), set `request.test_clock = true` to put the buyer on a Stripe test clock you can fast-forward to try renewals; the result returns its id as `test_clock_id`.

### Client options

```cpp
licensr::ClientConfig config;
config.api_key = "pk_live_...";
config.plugin_slug = "my-plugin";
config.base_url = "https://api.licensr.app";  // default; override for self-hosted/staging
config.device_id = stable_hwid;  // buckets rate limits per installation instead of per key — reuse your activate() identifier
config.origin = "app://my-plugin";  // rarely needed for native plugins; prefer setting the plugin's client_kind to "native" instead
config.timeout_ms = 10000;
config.retry = {/* max_retries */ 2, /* base_delay_ms */ 300, /* max_delay_ms */ 5000};  // network errors / 429 / 5xx, exponential backoff + jitter, honors Retry-After
config.transport = my_custom_transport;  // bring your own HTTP stack instead of the platform default
```

### Events

```cpp
client.events().on_validated = [](const licensr::ValidateResult& result) { /* ... */ };
client.events().on_retry = [](const std::string& method, int attempt, int delay_ms) { /* ... */ };
client.events().on_error = [](const std::string& method, const licensr::Error& error) { /* report to crash reporting */ };
```

Available hooks: `on_validated`, `on_activated`, `on_deactivated`, `on_token_issued`, `on_retry`, `on_error`. Because multiple `Client` instances for the same installation share one `ClientCore` (see above), event hooks are a broadcast registry — every live `Client`'s hooks fire for every request on that installation, not just the one that issued it.

### Errors

`licensr::Error` has an `ErrorKind`:

- `kApi` — non-2xx response with a parsed error body. `status` is the HTTP status, `code` is one of the documented error codes in `contract/conformance.yaml` (e.g. `license_inactive`, `origin_not_allowed`, `rate_limit_exceeded`) — branch on `code`, not `message`. `retry_after_seconds` is set on some `429` responses.
- `kNetwork` — the request never got a response: DNS/connect/TLS failure, timeout, or retries were exhausted.

## Hardware IDs

`licensr::compute_hardware_id(salt)` reads a platform-specific machine identifier — `IOPlatformUUID` (macOS/iOS), `MachineGuid` + system volume serial (Windows), `/etc/machine-id` (Linux) — and returns `sha256(salt + raw_id)` as a lowercase hex string, so the real platform identifier never leaves the process:

```cpp
#include <licensr/hwid.h>

std::string device_id = licensr::compute_hardware_id(config.plugin_slug).value_or(generate_and_persist_a_fallback_uuid());

config.device_id = device_id;  // buckets rate limits per installation

licensr::ActivateRequest request;
request.activation_type = licensr::ActivationType::kSeat;
request.identifier = device_id;  // ties the seat activation to this machine
```

Use a stable, plugin-specific `salt` (your `plugin_slug` is a reasonable default) — the same machine produces a different, uncorrelatable identifier for a different `salt`. `compute_hardware_id` returns `std::nullopt` under restricted permissions or an unusual sandbox; fall back to a self-generated UUID you persist yourself (e.g. via `FileTokenStore` below, or your plugin's own settings storage) in that case.

## Token storage

`FileTokenStore` (implements `TokenStore`) caches a string blob — typically the token from `Client::token`'s `TokenResult::token` — per license key, under a per-plugin directory in the platform's app-data location (`~/Library/Application Support/Licensr/<plugin_slug>/` on macOS, `%APPDATA%\Licensr\<plugin_slug>\` on Windows, `$XDG_DATA_HOME/licensr/<plugin_slug>/` on Linux):

```cpp
#include <licensr/token_store.h>

licensr::FileTokenStore store(config.plugin_slug);
store.save(license_key, token_result.token);

// Later, e.g. on next launch, before any network call has succeeded:
if (auto cached = store.load(license_key)) {
  // Offline verification (parsing + Ed25519 signature check) lands in a
  // follow-up SDK module — for now, treat a cache hit as "known-good as of
  // last successful token() call" and re-validate online to confirm it's
  // still current.
}
```

AUv3 app extensions run in their own sandboxed container, separate from the host app's — the default path above resolves to the _extension's_ container, not the host's. Pass an explicit `base_dir` (your app group's shared container, or the extension's own Application Support directory) in that case:

```cpp
licensr::FileTokenStore store(config.plugin_slug, auv3_container_path);
```

Bring your own `TokenStore` implementation instead if your host already has its own persistence (a DAW's plugin-state chunk, a keychain wrapper).

## HTTP transport

Licensr picks a platform default at CMake configure time (`LICENSR_HTTP_BACKEND`, default `AUTO`): WinHTTP on Windows, NSURLSession on macOS/iOS, libcurl on Linux. All three ride the OS's own TLS/certificate store rather than vendoring one. Set `LICENSR_HTTP_BACKEND=NONE` and supply `ClientConfig::transport` to plug in your own (a DAW host's networking callback, a game engine's HTTP client) — implement `licensr::HttpTransport`, whose single `perform()` method is called synchronously from Licensr's worker thread and must never be invoked from the audio thread.

## Vendored crypto

`src/detail/crypto/ed25519/` vendors the verify-only subset of [orlp/ed25519](https://github.com/orlp/ed25519) (itself a portable rewrite of the SUPERCOP "ref10" reference implementation) plus a SHA-512 implementation derived from LibTomCrypt, both public-domain/zlib-licensed — see `THIRD_PARTY_NOTICES.md`. Signing-only code paths (`ge_scalarmult_base`, the `base` precomputed table, `sc_muladd`, etc.) are stripped since this SDK only ever verifies EdDSA signatures produced by Licensr's backend, never creates them. Those files keep their upstream formatting (see the nested `.clang-format`) so future diffs against upstream stay minimal.

## JSON

`src/detail/json.h` is a minimal, dependency-free JSON value/parser/serializer scoped to the license API's known response shapes — no comments, no NaN/Infinity, no streaming. `include/licensr/Json` (the public type in `licensr.h`) is a separate, smaller class that exists only to carry the plugin-defined, arbitrarily-shaped `feature_flags` object across the API boundary without pulling a third-party JSON library into every consumer's link.

## Development

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build          # or: ./build/test/licensr_tests

# Vendored files under src/detail/crypto/ carry their own nested
# .clang-format (DisableFormat: true) and are skipped automatically.
find include src test \( -name '*.h' -o -name '*.cpp' -o -name '*.mm' \) -print0 | xargs -0 clang-format -i
```

Build options (`-D<name>=<value>`):

| Option                                     | Default                                    | Notes                                                           |
| ------------------------------------------ | ------------------------------------------ | --------------------------------------------------------------- |
| `LICENSR_BUILD_SHARED`                     | `OFF`                                      | Static by default; audio plugins typically want static linking. |
| `LICENSR_HTTP_BACKEND`                     | `AUTO`                                     | `WINHTTP`, `NSURLSESSION`, `CURL`, or `NONE` (bring your own).  |
| `LICENSR_BUILD_TESTS`                      | `ON`                                       | Builds `test/` and registers it with `ctest`.                   |
| `LICENSR_MSVC_RUNTIME_LIBRARY` (MSVC only) | `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL` | Match whatever CRT the rest of your plugin project uses.        |

For a macOS universal build: `cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

CI (`.github/workflows/test-sdk-cpp.yaml`) builds and runs this test suite on macOS (universal), Windows (MSVC), and Linux (GCC and Clang) on every push/PR that touches `sdks/cpp/**` or `contract/**`.

### Conformance suite

`test/test_conformance_live.cpp` (plus the minimal YAML parser in `test/conformance/`) runs every case in [`contract/conformance.yaml`](../../contract/README.md) against a real, running backend — the same contract `backend/tests/test_conformance_contract.py` and `sdks/js/test/conformance.test.ts` enforce. It's skipped by default (no live backend in a normal `ctest` run); to run it locally:

```bash
# 1. Start Postgres + migrate (from repo root)
docker compose up -d postgres
cd backend && APP_ENV=dev poetry run alembic upgrade head

# 2. Start the backend (BILLING_PROVIDER=mock avoids needing real Stripe creds)
APP_ENV=dev BILLING_PROVIDER=mock poetry run uvicorn main:app --port 8080

# 3. Seed fixtures for every case (in another shell)
poetry run python scripts/seed_conformance_fixtures.py --out /tmp/conformance_fixtures.json

# 4. Run the conformance suite (from sdks/cpp)
cd ../sdks/cpp
CONFORMANCE_BASE_URL=http://localhost:8080 \
CONFORMANCE_FIXTURES_PATH=/tmp/conformance_fixtures.json \
./build/test/licensr_tests
```

The `rate_limit_exceeded` case additionally requires `CONFORMANCE_RUN_SLOW=1` (it fires 61 requests to trip the bucket) — the dedicated `conformance` job in CI sets it; a plain local run skips that one case. See `.github/workflows/test-sdk-cpp.yaml` for how CI wires up the same four steps as one job.
