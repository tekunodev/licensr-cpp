// Vendored from orlp/ed25519 (https://github.com/orlp/ed25519), which in
// turn vendors this SHA-512 implementation from LibTomCrypt (Tom St Denis),
// public domain / WTFPL. See THIRD_PARTY_NOTICES.md at the sdks/cpp root.
#pragma once

#include <cstddef>
#include <cstdint>

namespace licensr::detail::ed25519_ref10 {

struct sha512_context {
  std::uint64_t length;
  std::uint64_t state[8];
  std::size_t curlen;
  unsigned char buf[128];
};

int sha512_init(sha512_context* md);
int sha512_final(sha512_context* md, unsigned char* out);
int sha512_update(sha512_context* md, const unsigned char* in, std::size_t inlen);
int sha512(const unsigned char* message, std::size_t message_len, unsigned char* out);

}  // namespace licensr::detail::ed25519_ref10
