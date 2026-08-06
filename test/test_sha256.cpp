// Known-answer tests against the standard NIST/FIPS 180-4 SHA-256 test
// vectors, exercised through our vendored LibTomCrypt-derived port.
#include <string>

#include "detail/crypto/sha256.h"
#include "test_framework.h"

namespace {

std::string to_hex(const unsigned char* bytes, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out.push_back(kHex[(bytes[i] >> 4) & 0xF]);
    out.push_back(kHex[bytes[i] & 0xF]);
  }
  return out;
}

std::string sha256_hex(const std::string& message) {
  unsigned char digest[32];
  licensr::detail::sha256(reinterpret_cast<const unsigned char*>(message.data()), message.size(), digest);
  return to_hex(digest, 32);
}

}  // namespace

LICENSR_TEST(sha256_of_empty_string) {
  LICENSR_EXPECT_EQ(sha256_hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

LICENSR_TEST(sha256_of_abc) {
  LICENSR_EXPECT_EQ(sha256_hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

LICENSR_TEST(sha256_of_a_message_spanning_multiple_blocks) {
  // FIPS 180-4 example 2: exercises the multi-block compress loop (the
  // 64-byte block boundary that single-block inputs like "abc" never hit).
  LICENSR_EXPECT_EQ(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                    std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

LICENSR_TEST(sha256_is_deterministic_and_input_sensitive) {
  LICENSR_EXPECT_EQ(sha256_hex("licensr"), sha256_hex("licensr"));
  LICENSR_EXPECT_TRUE(sha256_hex("licensr") != sha256_hex("Licensr"));
}
