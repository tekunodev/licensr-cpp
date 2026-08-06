// Small SHA-256 helpers shared by the platform HWID implementations
// (hwid_macos.cpp / hwid_windows.cpp / hwid_linux.cpp) and by
// FileTokenStore's cache-key derivation (token_store.cpp).
#pragma once

#include <string>

namespace licensr::detail {

// Lowercase-hex sha256(input).
std::string sha256_hex(const std::string& input);

// Lowercase-hex sha256(salt + raw_id) — only the "read the raw platform
// identifier" part differs per platform HWID implementation.
std::string hash_hardware_id(const std::string& salt, const std::string& raw_id);

}  // namespace licensr::detail
