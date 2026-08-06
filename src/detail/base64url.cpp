#include "base64url.h"

namespace licensr::detail {

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int decode_char(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '-') return 62;
  if (c == '_') return 63;
  return -1;
}

}  // namespace

std::string base64url_encode(const unsigned char* data, std::size_t len) {
  std::string out;
  out.reserve((len + 2) / 3 * 4);

  std::size_t i = 0;
  for (; i + 3 <= len; i += 3) {
    std::uint32_t chunk = (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                          static_cast<std::uint32_t>(data[i + 2]);
    out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
    out.push_back(kAlphabet[chunk & 0x3F]);
  }

  const std::size_t remaining = len - i;
  if (remaining == 1) {
    std::uint32_t chunk = static_cast<std::uint32_t>(data[i]) << 16;
    out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
  } else if (remaining == 2) {
    std::uint32_t chunk = (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8);
    out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
  }

  return out;
}

std::optional<std::vector<unsigned char>> base64url_decode(const std::string& input) {
  // Unpadded base64 never leaves a remainder of 1 when grouped by 4.
  if (input.size() % 4 == 1) {
    return std::nullopt;
  }

  std::vector<unsigned char> out;
  out.reserve(input.size() / 4 * 3 + 3);

  std::size_t i = 0;
  const std::size_t full_groups = input.size() / 4;
  for (std::size_t g = 0; g < full_groups; ++g, i += 4) {
    int a = decode_char(static_cast<unsigned char>(input[i]));
    int b = decode_char(static_cast<unsigned char>(input[i + 1]));
    int c = decode_char(static_cast<unsigned char>(input[i + 2]));
    int d = decode_char(static_cast<unsigned char>(input[i + 3]));
    if (a < 0 || b < 0 || c < 0 || d < 0) return std::nullopt;

    std::uint32_t chunk = (static_cast<std::uint32_t>(a) << 18) | (static_cast<std::uint32_t>(b) << 12) |
                          (static_cast<std::uint32_t>(c) << 6) | static_cast<std::uint32_t>(d);
    out.push_back(static_cast<unsigned char>((chunk >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((chunk >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(chunk & 0xFF));
  }

  const std::size_t remaining = input.size() - i;
  if (remaining == 2) {
    int a = decode_char(static_cast<unsigned char>(input[i]));
    int b = decode_char(static_cast<unsigned char>(input[i + 1]));
    if (a < 0 || b < 0) return std::nullopt;
    std::uint32_t chunk = (static_cast<std::uint32_t>(a) << 18) | (static_cast<std::uint32_t>(b) << 12);
    out.push_back(static_cast<unsigned char>((chunk >> 16) & 0xFF));
  } else if (remaining == 3) {
    int a = decode_char(static_cast<unsigned char>(input[i]));
    int b = decode_char(static_cast<unsigned char>(input[i + 1]));
    int c = decode_char(static_cast<unsigned char>(input[i + 2]));
    if (a < 0 || b < 0 || c < 0) return std::nullopt;
    std::uint32_t chunk = (static_cast<std::uint32_t>(a) << 18) | (static_cast<std::uint32_t>(b) << 12) |
                          (static_cast<std::uint32_t>(c) << 6);
    out.push_back(static_cast<unsigned char>((chunk >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((chunk >> 8) & 0xFF));
  } else if (remaining != 0) {
    return std::nullopt;
  }

  return out;
}

}  // namespace licensr::detail
