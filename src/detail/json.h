// Minimal JSON value + parser/serializer for the license API's response
// and request shapes. Deliberately not a general-purpose JSON library:
// no comments, no NaN/Infinity, no streaming, no user-facing pretty
// printing knobs. See the plan's "Crypto ... JSON" doctrine — no nlohmann.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace licensr::detail {

class Json {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  using Array = std::vector<Json>;
  // Object preserves insertion order (matches JSON source order); lookup
  // is O(n), which is irrelevant at the size of a license API response.
  using Object = std::vector<std::pair<std::string, Json>>;

  Json() : type_(Type::kNull) {}
  explicit Json(bool value) : type_(Type::kBool), bool_(value) {}
  explicit Json(double value) : type_(Type::kNumber), number_(value) {}
  explicit Json(int value) : type_(Type::kNumber), number_(value) {}
  explicit Json(std::int64_t value) : type_(Type::kNumber), number_(static_cast<double>(value)) {}
  explicit Json(std::string value) : type_(Type::kString), string_(std::move(value)) {}
  explicit Json(const char* value) : type_(Type::kString), string_(value) {}
  explicit Json(Array value) : type_(Type::kArray), array_(std::move(value)) {}
  explicit Json(Object value) : type_(Type::kObject), object_(std::move(value)) {}

  static Json make_object() { return Json(Object{}); }
  static Json make_array() { return Json(Array{}); }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::kNull; }
  bool is_bool() const { return type_ == Type::kBool; }
  bool is_number() const { return type_ == Type::kNumber; }
  bool is_string() const { return type_ == Type::kString; }
  bool is_array() const { return type_ == Type::kArray; }
  bool is_object() const { return type_ == Type::kObject; }

  bool as_bool(bool fallback = false) const { return type_ == Type::kBool ? bool_ : fallback; }
  double as_double(double fallback = 0.0) const { return type_ == Type::kNumber ? number_ : fallback; }
  std::int64_t as_int64(std::int64_t fallback = 0) const {
    return type_ == Type::kNumber ? static_cast<std::int64_t>(number_) : fallback;
  }
  const std::string& as_string() const {
    static const std::string kEmpty;
    return type_ == Type::kString ? string_ : kEmpty;
  }
  const Array& as_array() const {
    static const Array kEmpty;
    return type_ == Type::kArray ? array_ : kEmpty;
  }
  const Object& as_object() const {
    static const Object kEmpty;
    return type_ == Type::kObject ? object_ : kEmpty;
  }

  // Returns nullptr if this isn't an object or the key is absent.
  const Json* find(const std::string& key) const;

  // Convenience accessors for object fields; return the fallback if this
  // isn't an object, the key is absent, or the value has the wrong type.
  std::optional<std::string> get_string(const std::string& key) const;
  std::optional<bool> get_bool(const std::string& key) const;
  std::optional<double> get_number(const std::string& key) const;
  std::optional<std::int64_t> get_int64(const std::string& key) const;

  void set(std::string key, Json value);
  void push_back(Json value);

  std::string dump() const;

  static std::optional<Json> parse(const std::string& text);

 private:
  void dump_to(std::string& out) const;

  Type type_;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  Array array_;
  Object object_;
};

}  // namespace licensr::detail
