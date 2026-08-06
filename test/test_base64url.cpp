#include "detail/base64url.h"
#include "test_framework.h"

using licensr::detail::base64url_decode;
using licensr::detail::base64url_encode;

namespace {

std::vector<unsigned char> bytes_from_string(const std::string& s) {
  return std::vector<unsigned char>(s.begin(), s.end());
}

}  // namespace

LICENSR_TEST(encodes_without_padding) {
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("")), std::string(""));
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("f")), std::string("Zg"));
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("fo")), std::string("Zm8"));
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("foo")), std::string("Zm9v"));
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("foob")), std::string("Zm9vYg"));
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("fooba")), std::string("Zm9vYmE"));
  LICENSR_EXPECT_EQ(base64url_encode(bytes_from_string("foobar")), std::string("Zm9vYmFy"));
}

LICENSR_TEST(uses_url_safe_alphabet) {
  // 0xFB 0xFF 0xBF -> standard base64 would be "+/+/", url-safe is "-_-_".
  unsigned char data[] = {0xFB, 0xFF, 0xBF};
  LICENSR_EXPECT_EQ(base64url_encode(data, sizeof(data)), std::string("-_-_"));
}

LICENSR_TEST(decode_round_trips_through_encode) {
  for (const std::string& s :
       {std::string(""), std::string("f"), std::string("fo"), std::string("foo"), std::string("foob"),
        std::string("fooba"), std::string("foobar"), std::string("Licensr is not LicenseSeat.")}) {
    auto encoded = base64url_encode(bytes_from_string(s));
    auto decoded = base64url_decode(encoded);
    LICENSR_EXPECT_TRUE(decoded.has_value());
    if (decoded) {
      std::string round_tripped(decoded->begin(), decoded->end());
      LICENSR_EXPECT_EQ(round_tripped, s);
    }
  }
}

LICENSR_TEST(decode_rejects_invalid_characters) {
  LICENSR_EXPECT_FALSE(base64url_decode("not valid!!").has_value());
  LICENSR_EXPECT_FALSE(base64url_decode("Zm9v=").has_value());  // Padding not accepted.
}

LICENSR_TEST(decode_rejects_length_congruent_to_one_mod_four) {
  LICENSR_EXPECT_FALSE(base64url_decode("Z").has_value());
  LICENSR_EXPECT_FALSE(base64url_decode("ZZZZZ").has_value());
}
