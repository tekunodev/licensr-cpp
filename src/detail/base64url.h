// Unpadded base64url (RFC 4648 §5), the encoding used by JWT segments.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace licensr::detail {

std::string base64url_encode(const unsigned char* data, std::size_t len);
inline std::string base64url_encode(const std::vector<unsigned char>& data) {
  return base64url_encode(data.data(), data.size());
}

// Returns std::nullopt if `input` contains characters outside the
// base64url alphabet or has an invalid length for its (absent) padding.
std::optional<std::vector<unsigned char>> base64url_decode(const std::string& input);

}  // namespace licensr::detail
