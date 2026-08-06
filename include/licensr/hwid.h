// Salted, non-reversible hardware identifiers for seat activation and
// per-installation rate-limit bucketing. See docs/plugin-integration.md
// for how the license API uses `identifier` (ActivateRequest) and
// `device_id` (ClientConfig).
#pragma once

#include <optional>
#include <string>

#include "licensr/export.h"

namespace licensr {

// Reads a platform-specific hardware identifier — IOPlatformUUID on
// macOS/iOS, `MachineGuid` + system volume serial on Windows,
// `/etc/machine-id` on Linux — and returns `sha256(salt + raw_id)` as a
// lowercase hex string, so the actual platform identifier never leaves
// the process and two different `salt`s (e.g. two different plugins, or
// your own extra secret) never produce a correlatable identifier for the
// same machine.
//
// `salt` should be stable for your plugin (e.g. `plugin_slug`, or a
// constant baked into your build) — changing it changes every user's
// computed identifier, which for `ActivateRequest::identifier` means
// their existing seat activation would appear to be a different machine.
//
// Returns std::nullopt if the platform identifier couldn't be read (rare:
// restricted permissions, an unusual container/sandbox, or a platform
// with no compiled-in implementation). Callers should fall back to a
// self-generated, persisted UUID in that case — this function never
// blocks and never talks to the network, so there's nothing to retry.
LICENSR_API std::optional<std::string> compute_hardware_id(const std::string& salt);

}  // namespace licensr
