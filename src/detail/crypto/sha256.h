// Vendored from LibTomCrypt (Tom St Denis), public domain / WTFPL — same
// origin as sha512.h in crypto/ed25519/, see THIRD_PARTY_NOTICES.md at the
// sdks/cpp root. Kept separate from crypto/ed25519/ because it isn't part
// of the Ed25519 verify path: it exists solely to salt-and-hash the
// platform hardware identifier (see include/licensr/hwid.h) into a
// non-reversible device fingerprint.
#pragma once

#include <cstddef>
#include <cstdint>

namespace licensr::detail {

struct sha256_context {
  std::uint64_t length;
  std::uint32_t state[8];
  std::size_t curlen;
  unsigned char buf[64];
};

int sha256_init(sha256_context* md);
int sha256_update(sha256_context* md, const unsigned char* in, std::size_t inlen);
int sha256_final(sha256_context* md, unsigned char* out);
// Convenience one-shot: writes 32 bytes to `out`.
int sha256(const unsigned char* message, std::size_t message_len, unsigned char* out);

}  // namespace licensr::detail
