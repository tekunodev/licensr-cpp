#include "json.h"

#include <cmath>
#include <cstdio>

namespace licensr::detail {

const Json* Json::find(const std::string& key) const {
  if (type_ != Type::kObject) return nullptr;
  for (const auto& [k, v] : object_) {
    if (k == key) return &v;
  }
  return nullptr;
}

std::optional<std::string> Json::get_string(const std::string& key) const {
  const Json* v = find(key);
  if (v == nullptr || !v->is_string()) return std::nullopt;
  return v->as_string();
}

std::optional<bool> Json::get_bool(const std::string& key) const {
  const Json* v = find(key);
  if (v == nullptr || !v->is_bool()) return std::nullopt;
  return v->as_bool();
}

std::optional<double> Json::get_number(const std::string& key) const {
  const Json* v = find(key);
  if (v == nullptr || !v->is_number()) return std::nullopt;
  return v->as_double();
}

std::optional<std::int64_t> Json::get_int64(const std::string& key) const {
  const Json* v = find(key);
  if (v == nullptr || !v->is_number()) return std::nullopt;
  return v->as_int64();
}

void Json::set(std::string key, Json value) {
  if (type_ != Type::kObject) {
    type_ = Type::kObject;
    object_.clear();
  }
  for (auto& [k, v] : object_) {
    if (k == key) {
      v = std::move(value);
      return;
    }
  }
  object_.emplace_back(std::move(key), std::move(value));
}

void Json::push_back(Json value) {
  if (type_ != Type::kArray) {
    type_ = Type::kArray;
    array_.clear();
  }
  array_.push_back(std::move(value));
}

namespace {

void dump_string(const std::string& s, std::string& out) {
  out.push_back('"');
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  out.push_back('"');
}

void dump_number(double value, std::string& out) {
  // The license API's numeric fields are all integral (counts, unix
  // timestamps), so an integral fast path keeps output free of the
  // trailing ".0" that `%g`-style formatting would otherwise add.
  if (std::isfinite(value) && value == static_cast<double>(static_cast<std::int64_t>(value))) {
    out += std::to_string(static_cast<std::int64_t>(value));
    return;
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.17g", value);
  out += buf;
}

// Minimal recursive-descent JSON parser (RFC 8259).
class Parser {
 public:
  explicit Parser(const std::string& text) : text_(text) {}

  std::optional<Json> parse_document() {
    skip_whitespace();
    auto value = parse_value();
    if (!value) return std::nullopt;
    skip_whitespace();
    if (pos_ != text_.size()) return std::nullopt;  // Trailing garbage.
    return value;
  }

 private:
  void skip_whitespace() {
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool consume_literal(const char* literal) {
    std::size_t len = 0;
    while (literal[len] != '\0') ++len;
    if (text_.compare(pos_, len, literal) != 0) return false;
    pos_ += len;
    return true;
  }

  std::optional<Json> parse_value() {
    if (pos_ >= text_.size()) return std::nullopt;
    switch (text_[pos_]) {
      case '{':
        return parse_object();
      case '[':
        return parse_array();
      case '"':
        return parse_string_value();
      case 't':
        return consume_literal("true") ? std::optional<Json>(Json(true)) : std::nullopt;
      case 'f':
        return consume_literal("false") ? std::optional<Json>(Json(false)) : std::nullopt;
      case 'n':
        return consume_literal("null") ? std::optional<Json>(Json()) : std::nullopt;
      default:
        return parse_number();
    }
  }

  std::optional<Json> parse_object() {
    ++pos_;  // '{'
    Json::Object members;
    skip_whitespace();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      return Json(std::move(members));
    }
    while (true) {
      skip_whitespace();
      if (pos_ >= text_.size() || text_[pos_] != '"') return std::nullopt;
      auto key = parse_raw_string();
      if (!key) return std::nullopt;
      skip_whitespace();
      if (pos_ >= text_.size() || text_[pos_] != ':') return std::nullopt;
      ++pos_;
      skip_whitespace();
      auto value = parse_value();
      if (!value) return std::nullopt;
      members.emplace_back(std::move(*key), std::move(*value));
      skip_whitespace();
      if (pos_ >= text_.size()) return std::nullopt;
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == '}') {
        ++pos_;
        break;
      }
      return std::nullopt;
    }
    return Json(std::move(members));
  }

  std::optional<Json> parse_array() {
    ++pos_;  // '['
    Json::Array elements;
    skip_whitespace();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      ++pos_;
      return Json(std::move(elements));
    }
    while (true) {
      skip_whitespace();
      auto value = parse_value();
      if (!value) return std::nullopt;
      elements.push_back(std::move(*value));
      skip_whitespace();
      if (pos_ >= text_.size()) return std::nullopt;
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == ']') {
        ++pos_;
        break;
      }
      return std::nullopt;
    }
    return Json(std::move(elements));
  }

  std::optional<Json> parse_string_value() {
    auto s = parse_raw_string();
    if (!s) return std::nullopt;
    return Json(std::move(*s));
  }

  static void append_utf8(std::uint32_t codepoint, std::string& out) {
    if (codepoint <= 0x7F) {
      out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  std::optional<std::uint32_t> parse_hex4() {
    if (pos_ + 4 > text_.size()) return std::nullopt;
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      char c = text_[pos_ + i];
      value <<= 4;
      if (c >= '0' && c <= '9') {
        value |= static_cast<std::uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        value |= static_cast<std::uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        value |= static_cast<std::uint32_t>(c - 'A' + 10);
      } else {
        return std::nullopt;
      }
    }
    pos_ += 4;
    return value;
  }

  std::optional<std::string> parse_raw_string() {
    ++pos_;  // opening quote
    std::string out;
    while (true) {
      if (pos_ >= text_.size()) return std::nullopt;
      char c = text_[pos_];
      if (c == '"') {
        ++pos_;
        return out;
      }
      if (static_cast<unsigned char>(c) < 0x20) return std::nullopt;  // Unescaped control char.
      if (c != '\\') {
        out.push_back(c);
        ++pos_;
        continue;
      }
      ++pos_;  // backslash
      if (pos_ >= text_.size()) return std::nullopt;
      char esc = text_[pos_];
      switch (esc) {
        case '"':
          out.push_back('"');
          ++pos_;
          break;
        case '\\':
          out.push_back('\\');
          ++pos_;
          break;
        case '/':
          out.push_back('/');
          ++pos_;
          break;
        case 'b':
          out.push_back('\b');
          ++pos_;
          break;
        case 'f':
          out.push_back('\f');
          ++pos_;
          break;
        case 'n':
          out.push_back('\n');
          ++pos_;
          break;
        case 'r':
          out.push_back('\r');
          ++pos_;
          break;
        case 't':
          out.push_back('\t');
          ++pos_;
          break;
        case 'u': {
          ++pos_;
          auto codepoint = parse_hex4();
          if (!codepoint) return std::nullopt;
          std::uint32_t cp = *codepoint;
          // Surrogate pair.
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (pos_ + 1 >= text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') {
              return std::nullopt;
            }
            pos_ += 2;
            auto low = parse_hex4();
            if (!low || *low < 0xDC00 || *low > 0xDFFF) return std::nullopt;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (*low - 0xDC00);
          }
          append_utf8(cp, out);
          break;
        }
        default:
          return std::nullopt;
      }
    }
  }

  std::optional<Json> parse_number() {
    std::size_t start = pos_;
    if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
    if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return std::nullopt;
    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return std::nullopt;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
      if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return std::nullopt;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    }
    try {
      double value = std::stod(text_.substr(start, pos_ - start));
      return Json(value);
    } catch (...) {
      return std::nullopt;
    }
  }

  const std::string& text_;
  std::size_t pos_ = 0;
};

}  // namespace

void Json::dump_to(std::string& out) const {
  switch (type_) {
    case Type::kNull:
      out += "null";
      break;
    case Type::kBool:
      out += bool_ ? "true" : "false";
      break;
    case Type::kNumber:
      dump_number(number_, out);
      break;
    case Type::kString:
      dump_string(string_, out);
      break;
    case Type::kArray: {
      out.push_back('[');
      for (std::size_t i = 0; i < array_.size(); ++i) {
        if (i != 0) out.push_back(',');
        array_[i].dump_to(out);
      }
      out.push_back(']');
      break;
    }
    case Type::kObject: {
      out.push_back('{');
      for (std::size_t i = 0; i < object_.size(); ++i) {
        if (i != 0) out.push_back(',');
        dump_string(object_[i].first, out);
        out.push_back(':');
        object_[i].second.dump_to(out);
      }
      out.push_back('}');
      break;
    }
  }
}

std::string Json::dump() const {
  std::string out;
  dump_to(out);
  return out;
}

std::optional<Json> Json::parse(const std::string& text) {
  Parser parser(text);
  return parser.parse_document();
}

}  // namespace licensr::detail
