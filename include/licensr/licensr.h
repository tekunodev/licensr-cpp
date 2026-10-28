// Licensr C++ SDK — public API.
//
// Mirrors sdks/js/src/{client,types,errors}.ts field-for-field and
// method-for-method so the two SDKs read as siblings; see
// contract/openapi.json for the underlying wire contract. Naming follows
// C++ convention (snake_case) rather than JS's camelCase, but the shape
// — same methods, same fields, same error taxonomy — is identical.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "licensr/export.h"
#include "licensr/http_transport.h"

namespace licensr::detail {
class ClientCore;
}  // namespace licensr::detail

namespace licensr {

// A minimal JSON value, used only to carry the plugin-defined,
// arbitrarily-shaped `feature_flags` object through untouched. This is the
// one piece of `licensr::detail::Json` (see the "JSON" doctrine in
// sdks/cpp/README.md) exposed at the public API boundary, because there is
// no other way to hand a consumer an open-ended JSON object without
// pulling in a third-party JSON library.
class LICENSR_API Json {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;

  Json();
  explicit Json(bool value);
  explicit Json(double value);
  explicit Json(std::int64_t value);
  explicit Json(std::string value);
  explicit Json(Array value);
  explicit Json(Object value);

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::kNull; }
  bool is_bool() const { return type_ == Type::kBool; }
  bool is_number() const { return type_ == Type::kNumber; }
  bool is_string() const { return type_ == Type::kString; }
  bool is_array() const { return type_ == Type::kArray; }
  bool is_object() const { return type_ == Type::kObject; }

  bool as_bool(bool fallback = false) const;
  double as_double(double fallback = 0.0) const;
  std::int64_t as_int64(std::int64_t fallback = 0) const;
  const std::string& as_string() const;
  const Array& as_array() const;
  const Object& as_object() const;

  // Returns nullptr if this isn't an object or the key is absent.
  const Json* find(const std::string& key) const;

 private:
  Type type_ = Type::kNull;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  Array array_;
  Object object_;
};

enum class ActivationType { kSeat, kDomain, kUnknown };
enum class LicenseStatus { kActive, kInactive, kExpired, kUnknown };
// `kFull` when valid, `kNone` when cut off, `kLimited` when an expired license
// on a fallback plan keeps reduced access.
enum class Entitlement { kFull, kLimited, kNone, kUnknown };

LICENSR_API std::string to_string(ActivationType value);
LICENSR_API std::string to_string(LicenseStatus value);
LICENSR_API std::string to_string(Entitlement value);

// Information about the license. Nothing here decides access; safe to ignore.
struct LICENSR_API LicenseDetails {
  std::optional<std::string> plan_id;
  std::optional<ActivationType> activation_mode;
  // Devices or users allowed. Empty means unlimited.
  std::optional<std::int64_t> max_seats;
  // Websites allowed. Empty means unlimited.
  std::optional<std::int64_t> max_domains;
  std::optional<std::int64_t> seats_in_use;
  // When the license expires. Empty means it never expires.
  std::optional<std::string> expires_at;
};

struct LICENSR_API ValidateResult {
  // The answer you gate on: true unlocks your product, false keeps it locked.
  bool valid = false;
  // Why: pick the message to show the customer. Do not gate access on it.
  LicenseStatus status = LicenseStatus::kUnknown;
  // Optional. kLimited means expired but a fallback plan keeps reduced access.
  Entitlement entitlement = Entitlement::kUnknown;
  // Optional. Custom per-plan settings you defined.
  Json feature_flags;
  // Plan, limits and expiry. Informational only.
  LicenseDetails license;
};

struct LICENSR_API ActivateRequest {
  std::string license_key;
  ActivationType activation_type = ActivationType::kSeat;
  // Stable per-install identifier: HWID for kSeat, hostname for kDomain.
  std::string identifier;
  // Cosmetic, shown in admin/customer portals (e.g. "My MacBook Pro").
  std::optional<std::string> label;
};

// What was activated (an echo of your request). Informational only.
struct LICENSR_API ActivationInfo {
  ActivationType type = ActivationType::kSeat;
  std::string identifier;
};

struct LICENSR_API ActivateResult {
  // Save this: you need it to deactivate later.
  std::string activation_id;
  Json feature_flags;
  ActivationInfo activation;
};

struct LICENSR_API DeactivateResult {
  bool deactivated = false;
};

struct LICENSR_API ActivationListItem {
  std::string activation_id;
  ActivationType activation_type = ActivationType::kSeat;
  std::string identifier;
  std::optional<std::string> label;
  std::string first_seen_at;
  std::string last_seen_at;
};

struct LICENSR_API ActivationsResult {
  std::string license_id;
  std::vector<ActivationListItem> activations;
};

// How to verify the offline token and when to refresh it.
struct LICENSR_API TokenDetails {
  std::string algorithm;
  std::string kid;
  std::string issued_at;
  std::string expires_at;
  std::string jwks_url;
};

struct LICENSR_API TokenResult {
  // EdDSA-signed JWT. Verify against the plugin's JWKS (`details.jwks_url`) —
  // offline verification support lands in a follow-up SDK module.
  std::string token;
  TokenDetails details;
};

struct LICENSR_API CheckoutRequest {
  std::string plan_id;
  std::string customer_email;
  std::optional<std::string> success_url;
  std::optional<std::string> cancel_url;
  // Test keys (`pk_test_...`) only. Puts the buyer on a new Stripe test
  // clock you can fast-forward to try renewals. False leaves it out of the
  // request body.
  bool test_clock = false;
};

struct LICENSR_API CheckoutResult {
  std::string checkout_url;
  // The Stripe test clock the buyer is on, when `test_clock` asked for one.
  std::optional<std::string> test_clock_id;
};

enum class ErrorKind {
  // Non-2xx response with a parsed `detail.error` body. `code` is one of
  // the documented error codes in contract/conformance.yaml (e.g.
  // `license_inactive`, `origin_not_allowed`, `rate_limit_exceeded`).
  // Branch on `code`, not `message` — the message is for humans.
  kApi,
  // The request never got a response: DNS/connect/TLS failure, timeout,
  // or retries were exhausted.
  kNetwork,
};

struct LICENSR_API Error {
  ErrorKind kind = ErrorKind::kNetwork;
  int status = 0;    // HTTP status for kApi, 0 for kNetwork.
  std::string code;  // Empty for kNetwork.
  std::string message;
  std::optional<int> retry_after_seconds;  // Present on some 429 kApi errors.
};

// Either a value or an Error — never both. Mirrors the throw/catch split
// in the JS SDK (LicensrApiError / LicensrNetworkError) without requiring
// exceptions, since Licensr callbacks may run on the audio-adjacent
// worker thread and this SDK never throws across that boundary.
template <typename T>
class Result {
 public:
  static Result ok(T value) {
    Result r;
    r.value_ = std::move(value);
    return r;
  }
  static Result fail(Error error) {
    Result r;
    r.error_ = std::move(error);
    return r;
  }

  bool ok() const { return value_.has_value(); }
  explicit operator bool() const { return ok(); }

  const T& value() const { return *value_; }
  const Error& error() const { return *error_; }

 private:
  Result() = default;
  std::optional<T> value_;
  std::optional<Error> error_;
};

struct LICENSR_API RetryOptions {
  // Extra attempts after the first, on network errors / 429 / 5xx.
  int max_retries = 2;
  // Base delay for exponential backoff with full jitter.
  int base_delay_ms = 300;
  // Ceiling for the backoff delay.
  int max_delay_ms = 5000;
};

struct LICENSR_API ClientConfig {
  // Plugin API key (`pk_live_...` / `pk_test_...`) — see
  // docs/plugin-integration.md.
  std::string api_key;
  // The plugin this key belongs to (matches the slug in the admin dashboard).
  std::string plugin_slug;
  std::string base_url = "https://api.licensr.app";
  // Explicit `Origin` header override; leave unset for native plugins and
  // instead set the plugin's client type to "native" in the admin UI.
  std::optional<std::string> origin;
  // Stable per-install identifier (HWID). Buckets rate limits per
  // installation instead of per key. See sdks/cpp for HWID helpers.
  std::optional<std::string> device_id;
  // 0 means no timeout.
  int timeout_ms = 0;
  RetryOptions retry;
  // Bring your own HTTP stack instead of the platform default transport
  // selected at build time (WinHTTP / NSURLSession / libcurl).
  std::shared_ptr<HttpTransport> transport;
};

// Thin client for the Licensr public license API (`/v1/license/*`,
// `/v1/billing/checkout`).
//
// All network methods are asynchronous: they enqueue work onto a single
// background worker thread (shared across every Client in this process
// with the same api_key + plugin_slug + base_url — see the class comment
// on detail::ClientCore) and invoke `callback` from that worker thread
// once the request completes. Never call blocking methods from an audio
// callback; instead read `snapshot()`, which is safe to call from any
// thread including the audio thread.
class LICENSR_API Client {
 public:
  explicit Client(ClientConfig config);
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) noexcept;
  Client& operator=(Client&&) noexcept;

  // Lifecycle event hooks. Assign before issuing requests; each is called
  // from the worker thread. Leave unset if you don't need it.
  struct Events {
    std::function<void(const ValidateResult&)> on_validated;
    std::function<void(const ActivateResult&)> on_activated;
    std::function<void(const DeactivateResult&)> on_deactivated;
    std::function<void(const TokenResult&)> on_token_issued;
    std::function<void(const std::string& method, int attempt, int delay_ms)> on_retry;
    std::function<void(const std::string& method, const Error& error)> on_error;
  };
  Events& events();

  // Check whether a license is valid for this plugin, without minting an
  // offline token.
  void validate(const std::string& license_key, std::function<void(Result<ValidateResult>)> callback);

  // Run the same check as `validate`, and on success mint a short-lived
  // EdDSA-signed offline token (`token.details.jwks_url` publishes the verification
  // key).
  void token(const std::string& license_key, std::function<void(Result<TokenResult>)> callback);

  // Activate a seat (per-machine HWID) or domain for a license.
  void activate(const ActivateRequest& request, std::function<void(Result<ActivateResult>)> callback);

  // Release a previously created activation, freeing its seat/domain slot.
  void deactivate(const std::string& license_key, const std::string& activation_id,
                  std::function<void(Result<DeactivateResult>)> callback);

  // List every current activation for a license (seats/domains in use).
  void activations(const std::string& license_key, std::function<void(Result<ActivationsResult>)> callback);

  // Start a hosted checkout session for a plan; direct the user to the
  // returned URL.
  void checkout(const CheckoutRequest& request, std::function<void(Result<CheckoutResult>)> callback);

  // The most recent successful validate() result for this installation
  // (shared across every Client pointed at the same api_key + plugin_slug
  // + base_url — see the class comment on detail::ClientCore), or nullptr
  // if validate() hasn't completed successfully yet. Backed by an
  // atomically-swapped immutable snapshot — safe, non-blocking,
  // non-allocating to call from the audio thread on every `processBlock`.
  std::shared_ptr<const ValidateResult> snapshot() const;

 private:
  std::shared_ptr<detail::ClientCore> core_;
  std::shared_ptr<Events> events_;
};

}  // namespace licensr
