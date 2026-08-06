// Ported from orlp/ed25519's verify.c (zlib-licensed; see
// THIRD_PARTY_NOTICES.md at the sdks/cpp root), adapted to call into our
// namespaced, verify-only ref10 port.
#include "ed25519_verify.h"

#include "ge.h"
#include "sc.h"
#include "sha512.h"

namespace licensr::detail::ed25519_ref10 {

namespace {

bool consttime_equal(const unsigned char* x, const unsigned char* y) {
  unsigned char r = x[0] ^ y[0];
  for (int i = 1; i < 32; ++i) {
    r |= x[i] ^ y[i];
  }
  return r == 0;
}

}  // namespace

bool verify(const unsigned char* signature, const unsigned char* message, std::size_t message_len,
            const unsigned char* public_key) {
  unsigned char h[64];
  unsigned char checker[32];
  sha512_context hash;
  ge_p3 A;
  ge_p2 R;

  // High three bits of s must be zero (canonical scalar encoding).
  if (signature[63] & 224) {
    return false;
  }

  if (ge_frombytes_negate_vartime(&A, public_key) != 0) {
    return false;
  }

  sha512_init(&hash);
  sha512_update(&hash, signature, 32);
  sha512_update(&hash, public_key, 32);
  sha512_update(&hash, message, message_len);
  sha512_final(&hash, h);

  sc_reduce(h);
  ge_double_scalarmult_vartime(&R, h, &A, signature + 32);
  ge_tobytes(checker, &R);

  return consttime_equal(checker, signature);
}

}  // namespace licensr::detail::ed25519_ref10
