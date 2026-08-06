#include "licensr/http_transport.h"

#include <algorithm>
#include <cctype>

namespace licensr {

namespace {

bool equals_ignore_case(const std::string& a, const std::string& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

}  // namespace

const std::string* HttpResponse::find_header(const std::string& name) const {
  for (const auto& [header_name, header_value] : headers) {
    if (equals_ignore_case(header_name, name)) return &header_value;
  }
  return nullptr;
}

}  // namespace licensr
