// Thin, Licensr-owned wrapper around the vendored ref10 verify primitives.
// This is the only header outside this directory that should ever be
// included — everything else here is an implementation detail.
#pragma once

#include <cstddef>

namespace licensr::detail::ed25519_ref10 {

inline constexpr std::size_t kPublicKeyBytes = 32;
inline constexpr std::size_t kSignatureBytes = 64;

// Verifies an Ed25519 (EdDSA) signature over `message` using `public_key`.
// Returns true iff the signature is valid. Runs in variable time with
// respect to the (public) signature and public key, which is standard and
// safe for verification (only signing needs to be constant-time).
bool verify(const unsigned char* signature, const unsigned char* message, std::size_t message_len,
            const unsigned char* public_key);

}  // namespace licensr::detail::ed25519_ref10
