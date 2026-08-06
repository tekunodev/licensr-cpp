#include "detail/json.h"
#include "test_framework.h"

using licensr::detail::Json;

LICENSR_TEST(parses_primitives) {
  LICENSR_EXPECT_TRUE(Json::parse("null")->is_null());
  LICENSR_EXPECT_TRUE(Json::parse("true")->as_bool());
  LICENSR_EXPECT_FALSE(Json::parse("false")->as_bool(true));
  LICENSR_EXPECT_EQ(Json::parse("42")->as_int64(), int64_t{42});
  LICENSR_EXPECT_EQ(Json::parse("-17.5")->as_double(), -17.5);
  LICENSR_EXPECT_EQ(Json::parse("1.5e3")->as_double(), 1500.0);
  LICENSR_EXPECT_EQ(Json::parse("\"hello\"")->as_string(), std::string("hello"));
}

LICENSR_TEST(rejects_malformed_input) {
  LICENSR_EXPECT_FALSE(Json::parse("").has_value());
  LICENSR_EXPECT_FALSE(Json::parse("{").has_value());
  LICENSR_EXPECT_FALSE(Json::parse("{\"a\":}").has_value());
  LICENSR_EXPECT_FALSE(Json::parse("[1,2,]").has_value());
  LICENSR_EXPECT_FALSE(Json::parse("nul").has_value());
  LICENSR_EXPECT_FALSE(Json::parse("{} trailing").has_value());
}

LICENSR_TEST(parses_string_escapes_including_unicode_and_surrogate_pairs) {
  auto v = Json::parse("\"line\\nbreak\\ttab\\u0041\\u00e9\"");
  LICENSR_EXPECT_TRUE(v.has_value());
  const std::string expected_ascii_and_latin1 = "line\nbreak\ttabA\xC3\xA9";
  LICENSR_EXPECT_EQ(v->as_string(), expected_ascii_and_latin1);

  // \uD83D\uDE00 is the surrogate pair for U+1F600 (grinning face), UTF-8: F0 9F 98 80.
  auto emoji = Json::parse("\"\\uD83D\\uDE00\"");
  LICENSR_EXPECT_TRUE(emoji.has_value());
  const std::string expected = "\xF0\x9F\x98\x80";
  LICENSR_EXPECT_EQ(emoji->as_string(), expected);
}

LICENSR_TEST(parses_nested_objects_and_arrays_like_a_validate_response) {
  const std::string body = R"({
    "valid": true,
    "license": {
      "key": "LIC-ABC-123",
      "status": "active",
      "max_activations": 3,
      "feature_flags": {"pro": true, "seats": 5}
    },
    "activations": [
      {"device_id": "d1", "activated_at": "2026-01-01T00:00:00Z"},
      {"device_id": "d2", "activated_at": "2026-02-01T00:00:00Z"}
    ],
    "notes": null
  })";

  auto parsed = Json::parse(body);
  LICENSR_EXPECT_TRUE(parsed.has_value());
  if (!parsed) return;

  LICENSR_EXPECT_TRUE(parsed->get_bool("valid").value_or(false));

  const Json* license = parsed->find("license");
  LICENSR_EXPECT_TRUE(license != nullptr);
  if (license != nullptr) {
    LICENSR_EXPECT_EQ(license->get_string("key").value_or(""), std::string("LIC-ABC-123"));
    LICENSR_EXPECT_EQ(license->get_int64("max_activations").value_or(0), int64_t{3});

    const Json* flags = license->find("feature_flags");
    LICENSR_EXPECT_TRUE(flags != nullptr);
    if (flags != nullptr) {
      LICENSR_EXPECT_TRUE(flags->get_bool("pro").value_or(false));
      LICENSR_EXPECT_EQ(flags->get_int64("seats").value_or(0), int64_t{5});
    }
  }

  const Json* activations = parsed->find("activations");
  LICENSR_EXPECT_TRUE(activations != nullptr);
  if (activations != nullptr) {
    LICENSR_EXPECT_TRUE(activations->is_array());
    LICENSR_EXPECT_EQ(activations->as_array().size(), std::size_t{2});
    LICENSR_EXPECT_EQ(activations->as_array()[1].get_string("device_id").value_or(""), std::string("d2"));
  }

  const Json* notes = parsed->find("notes");
  LICENSR_EXPECT_TRUE(notes != nullptr);
  if (notes != nullptr) {
    LICENSR_EXPECT_TRUE(notes->is_null());
  }
}

LICENSR_TEST(dump_round_trips_through_parse) {
  Json obj = Json::make_object();
  obj.set("license_key", Json(std::string("LIC-ABC-123")));
  obj.set("device_id", Json(std::string("device-42")));
  obj.set("count", Json(std::int64_t{7}));
  obj.set("active", Json(true));

  Json arr = Json::make_array();
  arr.push_back(Json(std::int64_t{1}));
  arr.push_back(Json(std::int64_t{2}));
  obj.set("scopes_placeholder", arr);

  std::string dumped = obj.dump();
  auto reparsed = Json::parse(dumped);
  LICENSR_EXPECT_TRUE(reparsed.has_value());
  if (!reparsed) return;

  LICENSR_EXPECT_EQ(reparsed->get_string("license_key").value_or(""), std::string("LIC-ABC-123"));
  LICENSR_EXPECT_EQ(reparsed->get_string("device_id").value_or(""), std::string("device-42"));
  LICENSR_EXPECT_EQ(reparsed->get_int64("count").value_or(0), int64_t{7});
  LICENSR_EXPECT_TRUE(reparsed->get_bool("active").value_or(false));
}

LICENSR_TEST(dump_escapes_special_characters) {
  Json s(std::string("quote:\" backslash:\\ newline:\n tab:\t"));
  std::string dumped = s.dump();
  auto reparsed = Json::parse(dumped);
  LICENSR_EXPECT_TRUE(reparsed.has_value());
  if (reparsed) {
    LICENSR_EXPECT_EQ(reparsed->as_string(), s.as_string());
  }
}
