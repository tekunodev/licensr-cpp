#include "client.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>

#include "detail/json.h"
#include "singleton.h"

namespace licensr {

// Default HTTP transport factory, implemented by whichever platform
// backend was compiled in (see CMakeLists.txt's LICENSR_HTTP_BACKEND).
// Absent entirely when LICENSR_HTTP_BACKEND=NONE, in which case a caller
// must supply ClientConfig::transport.
#if defined(LICENSR_HTTP_WINHTTP) || defined(LICENSR_HTTP_NSURLSESSION) || defined(LICENSR_HTTP_CURL)
std::shared_ptr<HttpTransport> make_default_http_transport();
#endif

}  // namespace licensr

namespace licensr::detail {

namespace {

std::string url_encode(const std::string& value) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value) {
    bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                      c == '_' || c == '.' || c == '~';
    if (unreserved) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(kHex[(c >> 4) & 0xF]);
      out.push_back(kHex[c & 0xF]);
    }
  }
  return out;
}

// NOTE: explicitly qualified as `licensr::Json` throughout (rather than
// the bare `Json`) because this function lives inside `licensr::detail`,
// where unqualified `Json` would otherwise resolve to the *internal*
// `licensr::detail::Json` (see json.h) instead of the public API type
// declared in licensr.h.
licensr::Json to_public_json(const licensr::detail::Json& src) {
  switch (src.type()) {
    case licensr::detail::Json::Type::kNull:
      return licensr::Json();
    case licensr::detail::Json::Type::kBool:
      return licensr::Json(src.as_bool());
    case licensr::detail::Json::Type::kNumber:
      return licensr::Json(src.as_double());
    case licensr::detail::Json::Type::kString:
      return licensr::Json(src.as_string());
    case licensr::detail::Json::Type::kArray: {
      licensr::Json::Array out;
      out.reserve(src.as_array().size());
      for (const auto& element : src.as_array()) out.push_back(to_public_json(element));
      return licensr::Json(std::move(out));
    }
    case licensr::detail::Json::Type::kObject: {
      licensr::Json::Object out;
      out.reserve(src.as_object().size());
      for (const auto& [key, value] : src.as_object()) out.emplace_back(key, to_public_json(value));
      return licensr::Json(std::move(out));
    }
  }
  return licensr::Json();
}

ActivationType activation_type_from_string(const std::string& value) {
  if (value == "seat") return ActivationType::kSeat;
  if (value == "domain") return ActivationType::kDomain;
  return ActivationType::kUnknown;
}

LicenseStatus license_status_from_string(const std::string& value) {
  if (value == "active") return LicenseStatus::kActive;
  if (value == "inactive") return LicenseStatus::kInactive;
  if (value == "expired") return LicenseStatus::kExpired;
  return LicenseStatus::kUnknown;
}

Entitlement entitlement_from_string(const std::string& value) {
  if (value == "full") return Entitlement::kFull;
  if (value == "limited") return Entitlement::kLimited;
  if (value == "none") return Entitlement::kNone;
  return Entitlement::kUnknown;
}

std::optional<std::string> get_optional_string(const licensr::detail::Json& obj, const std::string& key) {
  const auto* v = obj.find(key);
  if (v == nullptr || !v->is_string()) return std::nullopt;
  return v->as_string();
}

std::optional<std::int64_t> get_optional_int64(const licensr::detail::Json& obj, const std::string& key) {
  const auto* v = obj.find(key);
  if (v == nullptr || !v->is_number()) return std::nullopt;
  return v->as_int64();
}

licensr::Json get_feature_flags(const licensr::detail::Json& obj) {
  const auto* v = obj.find("feature_flags");
  if (v == nullptr) return licensr::Json();
  return to_public_json(*v);
}

ValidateResult validate_result_from_wire(const licensr::detail::Json& wire) {
  ValidateResult result;
  result.valid = wire.get_bool("valid").value_or(false);
  result.status = license_status_from_string(wire.get_string("status").value_or(""));
  result.plan_id = get_optional_string(wire, "plan_id");
  result.plan_name = get_optional_string(wire, "plan_name");
  if (auto mode = get_optional_string(wire, "activation_mode")) {
    result.activation_mode = activation_type_from_string(*mode);
  }
  result.max_seats = get_optional_int64(wire, "max_seats");
  result.max_domains = get_optional_int64(wire, "max_domains");
  result.seats_in_use = get_optional_int64(wire, "seats_in_use");
  result.expires_at = get_optional_string(wire, "expires_at");
  result.entitlement = entitlement_from_string(wire.get_string("entitlement").value_or(""));
  result.fallback = wire.get_bool("fallback").value_or(false);
  result.feature_flags = get_feature_flags(wire);
  return result;
}

// Exponential backoff with full jitter (AWS-style): random(0, min(max, base * 2^attempt)).
int jitter_delay_ms(int attempt, int base_delay_ms, int max_delay_ms) {
  double cap = std::min<double>(max_delay_ms, static_cast<double>(base_delay_ms) * std::pow(2.0, attempt));
  static thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> dist(0.0, cap);
  return static_cast<int>(dist(rng));
}

std::optional<int> parse_retry_after_seconds(const HttpResponse& response) {
  const std::string* header = response.find_header("Retry-After");
  if (header == nullptr) return std::nullopt;
  try {
    int seconds = std::stoi(*header);
    if (seconds < 0) return std::nullopt;
    return seconds;
  } catch (...) {
    return std::nullopt;
  }
}

// Mirrors sdks/js/src/http.ts's parseErrorDetail: every hand-written
// HTTPException in the license/billing routers uses
// `{"detail": {"error": ..., "message": ...}}`; FastAPI/Pydantic's own 422
// validation shape is `{"detail": [{"msg": ..., "loc": [...]}]}`.
std::pair<std::string, std::string> parse_error_detail(const HttpResponse& response) {
  auto parsed = licensr::detail::Json::parse(response.body);
  if (!parsed) {
    return {"unknown_error",
            response.error_message.empty() ? "HTTP " + std::to_string(response.status) : response.error_message};
  }
  const auto* detail = parsed->find("detail");
  if (detail == nullptr) {
    return {"unknown_error", "HTTP " + std::to_string(response.status)};
  }
  if (detail->is_object()) {
    const auto* error = detail->find("error");
    if (error != nullptr && error->is_string()) {
      auto message = detail->get_string("message");
      return {error->as_string(), message.value_or("HTTP " + std::to_string(response.status))};
    }
  }
  if (detail->is_array() && !detail->as_array().empty()) {
    const auto& first = detail->as_array()[0];
    auto msg = first.get_string("msg");
    return {"validation_error", msg.value_or("Request validation failed")};
  }
  return {"unknown_error", "HTTP " + std::to_string(response.status)};
}

}  // namespace

ClientCore::ClientCore(ClientConfig config) : config_(std::move(config)) {
  if (config_.transport) {
    transport_ = config_.transport;
  } else {
#if defined(LICENSR_HTTP_WINHTTP) || defined(LICENSR_HTTP_NSURLSESSION) || defined(LICENSR_HTTP_CURL)
    transport_ = licensr::make_default_http_transport();
#else
    // No platform backend compiled in and no transport supplied: every
    // request will fail with a clear network error rather than crash.
    transport_ = nullptr;
#endif
  }
  worker_ = std::thread([this] { worker_loop(); });
}

ClientCore::~ClientCore() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stopping_ = true;
  }
  queue_cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void ClientCore::add_listener(const std::shared_ptr<Events>& events) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  listeners_.push_back(events);
}

void ClientCore::remove_listener(const std::shared_ptr<Events>& events) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                  [&events](const std::weak_ptr<Events>& weak) {
                                    auto locked = weak.lock();
                                    return !locked || locked == events;
                                  }),
                   listeners_.end());
}

std::vector<std::shared_ptr<ClientCore::Events>> ClientCore::live_listeners() const {
  std::vector<std::shared_ptr<Events>> out;
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  out.reserve(listeners_.size());
  for (const auto& weak : listeners_) {
    if (auto locked = weak.lock()) out.push_back(std::move(locked));
  }
  return out;
}

void ClientCore::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_.push_back(std::move(task));
  }
  queue_cv_.notify_one();
}

void ClientCore::worker_loop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
      if (queue_.empty()) {
        if (stopping_) return;
        continue;
      }
      task = std::move(queue_.front());
      queue_.pop_front();
    }
    task();
  }
}

std::string ClientCore::build_url(const std::string& path,
                                  const std::vector<std::pair<std::string, std::string>>& query) const {
  std::string url = config_.base_url;
  if (url.empty() || url.back() != '/') url.push_back('/');
  url += path;
  if (!query.empty()) {
    url.push_back('?');
    bool first = true;
    for (const auto& [key, value] : query) {
      if (!first) url.push_back('&');
      first = false;
      url += url_encode(key);
      url.push_back('=');
      url += url_encode(value);
    }
  }
  return url;
}

void ClientCore::emit_retry(const std::string& method_name, int attempt, int delay_ms) {
  for (const auto& events : live_listeners()) {
    if (events->on_retry) events->on_retry(method_name, attempt, delay_ms);
  }
}

void ClientCore::emit_error(const std::string& method_name, const Error& error) {
  for (const auto& events : live_listeners()) {
    if (events->on_error) events->on_error(method_name, error);
  }
}

void ClientCore::emit_validated(const ValidateResult& result) {
  for (const auto& events : live_listeners()) {
    if (events->on_validated) events->on_validated(result);
  }
}

void ClientCore::emit_activated(const ActivateResult& result) {
  for (const auto& events : live_listeners()) {
    if (events->on_activated) events->on_activated(result);
  }
}

void ClientCore::emit_deactivated(const DeactivateResult& result) {
  for (const auto& events : live_listeners()) {
    if (events->on_deactivated) events->on_deactivated(result);
  }
}

void ClientCore::emit_token_issued(const TokenResult& result) {
  for (const auto& events : live_listeners()) {
    if (events->on_token_issued) events->on_token_issued(result);
  }
}

void ClientCore::update_snapshot_from_validate(const ValidateResult& result) {
  std::atomic_store(&snapshot_, std::make_shared<const ValidateResult>(result));
}

std::shared_ptr<const ValidateResult> ClientCore::snapshot() const { return std::atomic_load(&snapshot_); }

Result<Json> ClientCore::perform_with_retry(const std::string& method_name, const PreparedRequest& request) {
  if (!transport_) {
    return Result<Json>::fail(Error{ErrorKind::kNetwork, 0, "",
                                    "No HTTP transport available. Build with an LICENSR_HTTP_BACKEND, or "
                                    "supply ClientConfig::transport.",
                                    std::nullopt});
  }

  const std::string url = build_url(request.path, request.query);
  const auto& retry = config_.retry;

  for (int attempt = 0;; ++attempt) {
    HttpRequest http_request;
    http_request.method = request.method;
    http_request.url = url;
    http_request.timeout_ms = config_.timeout_ms;
    http_request.headers.emplace_back("Authorization", "Bearer " + config_.api_key);
    if (!request.body_json.empty()) {
      http_request.headers.emplace_back("Content-Type", "application/json");
      http_request.body = request.body_json;
    }
    if (config_.device_id) {
      http_request.headers.emplace_back("X-Licensr-Device-Id", *config_.device_id);
    }
    if (config_.origin) {
      http_request.headers.emplace_back("Origin", *config_.origin);
    }

    HttpResponse response = transport_->perform(http_request);

    if (response.status == 0) {
      if (attempt < retry.max_retries) {
        int delay_ms = jitter_delay_ms(attempt, retry.base_delay_ms, retry.max_delay_ms);
        emit_retry(method_name, attempt + 1, delay_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        continue;
      }
      Error error{ErrorKind::kNetwork, 0, "",
                  "Request to " + request.path + " failed after " + std::to_string(attempt + 1) + " attempt(s)" +
                      (response.error_message.empty() ? "" : ": " + response.error_message),
                  std::nullopt};
      emit_error(method_name, error);
      return Result<Json>::fail(error);
    }

    if (response.status >= 200 && response.status < 300) {
      if (response.status == 204 || response.body.empty()) {
        return Result<Json>::ok(licensr::detail::Json::make_object());
      }
      auto parsed = licensr::detail::Json::parse(response.body);
      if (!parsed) {
        Error error{ErrorKind::kApi, response.status, "unknown_error", "Response body was not valid JSON",
                    std::nullopt};
        emit_error(method_name, error);
        return Result<Json>::fail(error);
      }
      return Result<Json>::ok(std::move(*parsed));
    }

    const bool retriable = response.status == 429 || response.status >= 500;
    if (retriable && attempt < retry.max_retries) {
      auto retry_after = parse_retry_after_seconds(response);
      int delay_ms =
          retry_after ? *retry_after * 1000 : jitter_delay_ms(attempt, retry.base_delay_ms, retry.max_delay_ms);
      emit_retry(method_name, attempt + 1, delay_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      continue;
    }

    auto [code, message] = parse_error_detail(response);
    Error error{ErrorKind::kApi, response.status, code, message, parse_retry_after_seconds(response)};
    emit_error(method_name, error);
    return Result<Json>::fail(error);
  }
}

void ClientCore::validate(const std::string& license_key, std::function<void(Result<ValidateResult>)> callback) {
  licensr::detail::Json body = licensr::detail::Json::make_object();
  body.set("license_key", licensr::detail::Json(license_key));
  body.set("plugin_slug", licensr::detail::Json(config_.plugin_slug));

  enqueue([this, body = body.dump(), callback = std::move(callback)]() {
    auto result = perform_with_retry("validate", PreparedRequest{HttpMethod::kPost, "v1/license/validate", {}, body});
    if (!result.ok()) {
      callback(Result<ValidateResult>::fail(result.error()));
      return;
    }
    ValidateResult validate_result = validate_result_from_wire(result.value());
    update_snapshot_from_validate(validate_result);
    emit_validated(validate_result);
    callback(Result<ValidateResult>::ok(std::move(validate_result)));
  });
}

void ClientCore::token(const std::string& license_key, std::function<void(Result<TokenResult>)> callback) {
  licensr::detail::Json body = licensr::detail::Json::make_object();
  body.set("license_key", licensr::detail::Json(license_key));
  body.set("plugin_slug", licensr::detail::Json(config_.plugin_slug));

  enqueue([this, body = body.dump(), callback = std::move(callback)]() {
    auto result = perform_with_retry("token", PreparedRequest{HttpMethod::kPost, "v1/license/token", {}, body});
    if (!result.ok()) {
      callback(Result<TokenResult>::fail(result.error()));
      return;
    }
    const auto& wire = result.value();
    TokenResult token_result;
    token_result.token = wire.get_string("token").value_or("");
    token_result.algorithm = wire.get_string("algorithm").value_or("");
    token_result.kid = wire.get_string("kid").value_or("");
    token_result.issued_at = wire.get_string("issued_at").value_or("");
    token_result.expires_at = wire.get_string("expires_at").value_or("");
    token_result.jwks_url = wire.get_string("jwks_url").value_or("");
    emit_token_issued(token_result);
    callback(Result<TokenResult>::ok(std::move(token_result)));
  });
}

void ClientCore::activate(const ActivateRequest& request, std::function<void(Result<ActivateResult>)> callback) {
  licensr::detail::Json body = licensr::detail::Json::make_object();
  body.set("license_key", licensr::detail::Json(request.license_key));
  body.set("plugin_slug", licensr::detail::Json(config_.plugin_slug));
  body.set("activation_type", licensr::detail::Json(to_string(request.activation_type)));
  body.set("identifier", licensr::detail::Json(request.identifier));
  if (request.label) {
    body.set("label", licensr::detail::Json(*request.label));
  } else {
    body.set("label", licensr::detail::Json());
  }

  enqueue([this, body = body.dump(), callback = std::move(callback)]() {
    auto result = perform_with_retry("activate", PreparedRequest{HttpMethod::kPost, "v1/license/activate", {}, body});
    if (!result.ok()) {
      callback(Result<ActivateResult>::fail(result.error()));
      return;
    }
    const auto& wire = result.value();
    ActivateResult activate_result;
    activate_result.activation_id = wire.get_string("activation_id").value_or("");
    activate_result.activation_type = activation_type_from_string(wire.get_string("activation_type").value_or(""));
    activate_result.identifier = wire.get_string("identifier").value_or("");
    activate_result.feature_flags = get_feature_flags(wire);
    emit_activated(activate_result);
    callback(Result<ActivateResult>::ok(std::move(activate_result)));
  });
}

void ClientCore::deactivate(const std::string& license_key, const std::string& activation_id,
                            std::function<void(Result<DeactivateResult>)> callback) {
  licensr::detail::Json body = licensr::detail::Json::make_object();
  body.set("license_key", licensr::detail::Json(license_key));
  body.set("activation_id", licensr::detail::Json(activation_id));

  enqueue([this, body = body.dump(), callback = std::move(callback)]() {
    auto result =
        perform_with_retry("deactivate", PreparedRequest{HttpMethod::kPost, "v1/license/deactivate", {}, body});
    if (!result.ok()) {
      callback(Result<DeactivateResult>::fail(result.error()));
      return;
    }
    DeactivateResult deactivate_result;
    deactivate_result.deactivated = result.value().get_bool("deactivated").value_or(false);
    emit_deactivated(deactivate_result);
    callback(Result<DeactivateResult>::ok(std::move(deactivate_result)));
  });
}

void ClientCore::activations(const std::string& license_key, std::function<void(Result<ActivationsResult>)> callback) {
  std::vector<std::pair<std::string, std::string>> query{{"license_key", license_key},
                                                         {"plugin_slug", config_.plugin_slug}};

  enqueue([this, query, callback = std::move(callback)]() {
    auto result =
        perform_with_retry("activations", PreparedRequest{HttpMethod::kGet, "v1/license/activations", query, ""});
    if (!result.ok()) {
      callback(Result<ActivationsResult>::fail(result.error()));
      return;
    }
    const auto& wire = result.value();
    ActivationsResult activations_result;
    activations_result.license_id = wire.get_string("license_id").value_or("");
    const auto* items = wire.find("activations");
    if (items != nullptr && items->is_array()) {
      activations_result.activations.reserve(items->as_array().size());
      for (const auto& item : items->as_array()) {
        ActivationListItem list_item;
        list_item.activation_id = item.get_string("activation_id").value_or("");
        list_item.activation_type = activation_type_from_string(item.get_string("activation_type").value_or(""));
        list_item.identifier = item.get_string("identifier").value_or("");
        list_item.label = get_optional_string(item, "label");
        list_item.first_seen_at = item.get_string("first_seen_at").value_or("");
        list_item.last_seen_at = item.get_string("last_seen_at").value_or("");
        activations_result.activations.push_back(std::move(list_item));
      }
    }
    callback(Result<ActivationsResult>::ok(std::move(activations_result)));
  });
}

void ClientCore::checkout(const CheckoutRequest& request, std::function<void(Result<CheckoutResult>)> callback) {
  licensr::detail::Json body = licensr::detail::Json::make_object();
  body.set("plan_id", licensr::detail::Json(request.plan_id));
  body.set("customer_email", licensr::detail::Json(request.customer_email));
  body.set("success_url", request.success_url ? licensr::detail::Json(*request.success_url) : licensr::detail::Json());
  body.set("cancel_url", request.cancel_url ? licensr::detail::Json(*request.cancel_url) : licensr::detail::Json());

  enqueue([this, body = body.dump(), callback = std::move(callback)]() {
    auto result = perform_with_retry("checkout", PreparedRequest{HttpMethod::kPost, "v1/billing/checkout", {}, body});
    if (!result.ok()) {
      callback(Result<CheckoutResult>::fail(result.error()));
      return;
    }
    CheckoutResult checkout_result;
    checkout_result.checkout_url = result.value().get_string("checkout_url").value_or("");
    callback(Result<CheckoutResult>::ok(std::move(checkout_result)));
  });
}

namespace {

struct ClientCoreKeyHash {
  std::size_t operator()(const ClientCoreKey& key) const {
    std::size_t h1 = std::hash<std::string>{}(key.api_key);
    std::size_t h2 = std::hash<std::string>{}(key.plugin_slug);
    std::size_t h3 = std::hash<std::string>{}(key.base_url);
    // Boost-style hash_combine.
    std::size_t seed = h1;
    seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};

RefcountedRegistry<ClientCoreKey, ClientCore, ClientCoreKeyHash>& client_core_registry() {
  static RefcountedRegistry<ClientCoreKey, ClientCore, ClientCoreKeyHash> registry;
  return registry;
}

}  // namespace

std::shared_ptr<ClientCore> acquire_client_core(ClientConfig config) {
  ClientCoreKey key{config.api_key, config.plugin_slug, config.base_url};
  return client_core_registry().acquire(
      key, [config = std::move(config)]() mutable { return new ClientCore(std::move(config)); });
}

}  // namespace licensr::detail
