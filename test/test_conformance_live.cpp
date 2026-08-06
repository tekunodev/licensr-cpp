// Native C++ conformance runner for contract/conformance.yaml — the third
// of one per SDK (see backend/scripts/seed_conformance_fixtures.py's own
// doc comment), mirroring sdks/js/test/conformance.test.ts case-for-case
// against a *running* backend, using licensr::HttpTransport directly
// rather than licensr::Client since a few cases exercise things the
// client intentionally doesn't expose (a missing Authorization header, a
// deprecated path). See contract/README.md for how to start the backend
// and produce the fixtures file this reads.
//
// Skipped by default: requires CONFORMANCE_BASE_URL (the running backend)
// and CONFORMANCE_FIXTURES_PATH (JSON from
// backend/scripts/seed_conformance_fixtures.py), matching the JS runner's
// own opt-in.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "conformance/yaml_lite.h"
#include "detail/json.h"
#include "licensr/http_transport.h"
#include "test_framework.h"

#ifndef LICENSR_CONFORMANCE_YAML_PATH
#error "LICENSR_CONFORMANCE_YAML_PATH must be set by CMake to contract/conformance.yaml's absolute path"
#endif

namespace licensr {
// Same forward declaration client.cpp uses for the platform-selected
// transport (see its own comment) — this test wants the real thing, not
// the stub test_client.cpp uses to avoid the network entirely.
std::shared_ptr<HttpTransport> make_default_http_transport();
}  // namespace licensr

namespace {

using licensr::detail::Json;

std::string read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("cannot open " + path);
  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

std::string resolve_placeholders(const std::string& value, const Json& fixture_values) {
  std::string result = value;
  for (const auto& [name, replacement] : fixture_values.as_object()) {
    std::string placeholder = "${" + name + "}";
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.size(), replacement.as_string());
      pos += replacement.as_string().size();
    }
  }
  return result;
}

Json resolve_placeholders_json(const Json& value, const Json& fixture_values) {
  switch (value.type()) {
    case Json::Type::kString:
      return Json(resolve_placeholders(value.as_string(), fixture_values));
    case Json::Type::kArray: {
      auto array = Json::make_array();
      for (const auto& item : value.as_array()) array.push_back(resolve_placeholders_json(item, fixture_values));
      return array;
    }
    case Json::Type::kObject: {
      auto object = Json::make_object();
      for (const auto& [key, item] : value.as_object()) {
        object.set(key, resolve_placeholders_json(item, fixture_values));
      }
      return object;
    }
    default:
      return value;
  }
}

// Reads a dotted path like "detail.error" or "keys.0.kty" out of a parsed
// JSON body. Returns nullptr if any segment is absent or out of range.
const Json* get_path(const Json& root, const std::string& dotted) {
  const Json* current = &root;
  size_t start = 0;
  while (start <= dotted.size()) {
    size_t dot = dotted.find('.', start);
    std::string part = dotted.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

    if (current->is_array()) {
      auto index = static_cast<size_t>(std::stoul(part));
      if (index >= current->as_array().size()) return nullptr;
      current = &current->as_array()[index];
    } else if (current->is_object()) {
      current = current->find(part);
      if (current == nullptr) return nullptr;
    } else {
      return nullptr;
    }

    if (dot == std::string::npos) break;
    start = dot + 1;
  }
  return current;
}

bool json_equals(const Json& a, const Json& b) {
  if (a.type() != b.type()) return false;
  switch (a.type()) {
    case Json::Type::kNull:
      return true;
    case Json::Type::kBool:
      return a.as_bool() == b.as_bool();
    case Json::Type::kNumber:
      return a.as_double() == b.as_double();
    case Json::Type::kString:
      return a.as_string() == b.as_string();
    case Json::Type::kArray: {
      if (a.as_array().size() != b.as_array().size()) return false;
      for (size_t i = 0; i < a.as_array().size(); ++i) {
        if (!json_equals(a.as_array()[i], b.as_array()[i])) return false;
      }
      return true;
    }
    case Json::Type::kObject: {
      if (a.as_object().size() != b.as_object().size()) return false;
      for (const auto& [key, value] : a.as_object()) {
        const Json* other = b.find(key);
        if (other == nullptr || !json_equals(value, *other)) return false;
      }
      return true;
    }
  }
  return false;
}

using Header = std::pair<std::string, std::string>;

std::optional<Header> auth_header(const std::string& auth, const Json& values) {
  if (auth == "missing" || auth == "none") return std::nullopt;
  if (auth == "invalid") return Header{"Authorization", "Bearer pk_live_00000000000000000000000000"};
  if (auth == "valid_api_key" || auth == "revoked") {
    auto api_key = values.get_string("api_key");
    if (!api_key) throw std::runtime_error("fixtures missing api_key");
    return Header{"Authorization", "Bearer " + *api_key};
  }
  throw std::runtime_error("unknown auth kind: " + auth);
}

std::optional<Header> origin_header(const std::string& origin, const Json& values) {
  if (origin.empty() || origin == "omit") return std::nullopt;
  if (origin == "allowed") {
    auto allowed = values.get_string("_allowed_origin");
    if (!allowed) throw std::runtime_error("fixtures missing _allowed_origin");
    return Header{"Origin", *allowed};
  }
  if (origin == "disallowed") return Header{"Origin", "https://evil.example"};
  throw std::runtime_error("unknown origin kind: " + origin);
}

licensr::HttpResponse send_case(licensr::HttpTransport& transport, const std::string& base_url, const Json& request,
                                const Json& values) {
  licensr::HttpRequest http_request;
  http_request.url = base_url + resolve_placeholders(request.get_string("path").value_or(""), values);

  if (auto auth = auth_header(request.get_string("auth").value_or(""), values)) http_request.headers.push_back(*auth);
  if (auto origin = origin_header(request.get_string("origin").value_or(""), values)) {
    http_request.headers.push_back(*origin);
  }

  if (request.get_string("method").value_or("GET") == "GET") {
    http_request.method = licensr::HttpMethod::kGet;
  } else {
    http_request.method = licensr::HttpMethod::kPost;
    const Json* body = request.find("body");
    Json resolved_body = body != nullptr ? resolve_placeholders_json(*body, values) : Json::make_object();
    http_request.body = resolved_body.dump();
    http_request.headers.emplace_back("Content-Type", "application/json");
  }

  auto repeat = request.get_int64("repeat").value_or(1);
  licensr::HttpResponse response;
  for (std::int64_t i = 0; i < repeat; ++i) response = transport.perform(http_request);
  return response;
}

void run_case(const Json& test_case, licensr::HttpTransport& transport, const std::string& base_url,
              const Json& fixtures) {
  auto id = test_case.get_string("id").value_or("<missing id>");
  const Json* request = test_case.find("request");
  const Json* expect = test_case.find("expect");
  if (request == nullptr || expect == nullptr) {
    ::licensr::test::report_failure(__FILE__, __LINE__, "conformance case " + id + ": missing request/expect");
    return;
  }

  // Documented in contract/conformance.yaml as informational for SDK
  // runners against a shared tenant — fires 61 real requests and
  // exhausts a rate-limit bucket, so it's opt-in rather than run by
  // default.
  if (id == "rate_limit_exceeded" && std::getenv("CONFORMANCE_RUN_SLOW") == nullptr) {
    std::fprintf(stderr, "SKIP conformance case %s (set CONFORMANCE_RUN_SLOW=1 to include it)\n", id.c_str());
    return;
  }

  const Json* values = fixtures.find(id);
  if (values == nullptr) {
    ::licensr::test::report_failure(
        __FILE__, __LINE__, "conformance case " + id + ": no seeded fixtures — re-run seed_conformance_fixtures.py");
    return;
  }

  try {
    auto response = send_case(transport, base_url, *request, *values);
    auto expected_status = expect->get_int64("status").value_or(-1);
    if (response.status != expected_status) {
      ::licensr::test::report_failure(__FILE__, __LINE__,
                                      "conformance case " + id + ": expected status " +
                                          std::to_string(expected_status) + ", got " + std::to_string(response.status) +
                                          " (body: " + response.body + ")");
      return;
    }

    const Json* expected_fields = expect->find("json");
    if (expected_fields == nullptr) return;

    auto body_json = Json::parse(response.body);
    for (const auto& [path, expected_value] : expected_fields->as_object()) {
      const Json* actual = body_json ? get_path(*body_json, path) : nullptr;
      if (actual == nullptr || !json_equals(*actual, expected_value)) {
        ::licensr::test::report_failure(__FILE__, __LINE__,
                                        "conformance case " + id + ": expected " + expected_value.dump() + " at " +
                                            path + ", got " + (actual != nullptr ? actual->dump() : "<missing>"));
      }
    }
  } catch (const std::exception& error) {
    ::licensr::test::report_failure(__FILE__, __LINE__, "conformance case " + id + ": " + std::string(error.what()));
  }
}

}  // namespace

LICENSR_TEST(conformance_suite) {
  const char* base_url = std::getenv("CONFORMANCE_BASE_URL");
  const char* fixtures_path = std::getenv("CONFORMANCE_FIXTURES_PATH");
  if (base_url == nullptr || fixtures_path == nullptr) {
    std::fprintf(stderr,
                 "SKIP conformance_suite: set CONFORMANCE_BASE_URL and CONFORMANCE_FIXTURES_PATH to run against a "
                 "live backend (see contract/README.md)\n");
    return;
  }

  try {
    auto document = licensr_conformance::parse_yaml(read_file(LICENSR_CONFORMANCE_YAML_PATH));
    const Json* cases = document.find("cases");
    if (cases == nullptr || !cases->is_array()) {
      throw std::runtime_error("conformance.yaml missing a top-level 'cases' sequence");
    }

    auto fixtures = Json::parse(read_file(fixtures_path));
    if (!fixtures) throw std::runtime_error("failed to parse fixtures JSON at " + std::string(fixtures_path));

    auto transport = licensr::make_default_http_transport();
    if (!transport) throw std::runtime_error("no default HTTP transport compiled in");

    for (const auto& test_case : cases->as_array()) run_case(test_case, *transport, base_url, *fixtures);
  } catch (const std::exception& error) {
    ::licensr::test::report_failure(__FILE__, __LINE__, std::string("conformance suite setup failed: ") + error.what());
  }
}
