#include "hwid_support.h"

#include "detail/crypto/sha256.h"

namespace licensr::detail {

std::string sha256_hex(const std::string& input) {
  unsigned char digest[32];
  sha256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);

  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(sizeof(digest) * 2);
  for (unsigned char byte : digest) {
    hex.push_back(kHex[(byte >> 4) & 0xF]);
    hex.push_back(kHex[byte & 0xF]);
  }
  return hex;
}

std::string hash_hardware_id(const std::string& salt, const std::string& raw_id) { return sha256_hex(salt + raw_id); }

}  // namespace licensr::detail
