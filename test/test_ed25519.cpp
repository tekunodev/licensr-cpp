// Known-answer tests against RFC 8032 section 7.1 Ed25519 test vectors,
// exercised through our namespaced, trimmed ref10 port.
#include "detail/crypto/ed25519/ed25519_verify.h"
#include "test_framework.h"

namespace {

constexpr int kHexPairShift = 4;

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void hex_to_bytes(const char* hex, unsigned char* out, std::size_t out_len) {
  for (std::size_t i = 0; i < out_len; ++i) {
    out[i] = static_cast<unsigned char>((hex_nibble(hex[2 * i]) << kHexPairShift) | hex_nibble(hex[2 * i + 1]));
  }
}

}  // namespace

LICENSR_TEST(rfc8032_vector1_empty_message) {
  unsigned char pk[32];
  unsigned char sig[64];
  hex_to_bytes("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", pk, 32);
  hex_to_bytes(
      "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901"
      "555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a1"
      "00b",
      sig, 64);

  const unsigned char* message = nullptr;
  LICENSR_EXPECT_TRUE(licensr::detail::ed25519_ref10::verify(sig, message, 0, pk));
}

LICENSR_TEST(rfc8032_vector2_one_byte_message) {
  unsigned char pk[32];
  unsigned char sig[64];
  unsigned char message[1];
  hex_to_bytes("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", pk, 32);
  hex_to_bytes(
      "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69d"
      "a085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c"
      "00",
      sig, 64);
  hex_to_bytes("72", message, 1);

  LICENSR_EXPECT_TRUE(licensr::detail::ed25519_ref10::verify(sig, message, 1, pk));
}

LICENSR_TEST(rfc8032_vector3_two_byte_message) {
  unsigned char pk[32];
  unsigned char sig[64];
  unsigned char message[2];
  hex_to_bytes("fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", pk, 32);
  hex_to_bytes(
      "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3a"
      "c18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec4"
      "0a",
      sig, 64);
  hex_to_bytes("af82", message, 2);

  LICENSR_EXPECT_TRUE(licensr::detail::ed25519_ref10::verify(sig, message, 2, pk));
}

LICENSR_TEST(rejects_flipped_message_byte) {
  unsigned char pk[32];
  unsigned char sig[64];
  unsigned char message[1];
  hex_to_bytes("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", pk, 32);
  hex_to_bytes(
      "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69d"
      "a085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c"
      "00",
      sig, 64);
  hex_to_bytes("73", message, 1);  // Flip the message byte that was signed.

  LICENSR_EXPECT_FALSE(licensr::detail::ed25519_ref10::verify(sig, message, 1, pk));
}

LICENSR_TEST(rejects_signature_with_high_bits_set) {
  unsigned char pk[32];
  unsigned char sig[64];
  hex_to_bytes("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", pk, 32);
  hex_to_bytes(
      "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901"
      "555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a1"
      "00b",
      sig, 64);
  sig[63] |= 0xE0;  // Non-canonical scalar encoding must be rejected.

  const unsigned char* message = nullptr;
  LICENSR_EXPECT_FALSE(licensr::detail::ed25519_ref10::verify(sig, message, 0, pk));
}
