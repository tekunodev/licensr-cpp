// Exercises licensr::Client end-to-end against a stub HttpTransport: wire
// JSON -> public struct mapping, retry/backoff on 5xx and network errors,
// API error parsing, and the multi-Client event-listener registry (see the
// class comment on detail::ClientCore for why that last one matters).
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "licensr/licensr.h"
#include "test_framework.h"

namespace {

using licensr::HttpMethod;
using licensr::HttpRequest;
using licensr::HttpResponse;
using licensr::HttpTransport;

class StubHttpTransport : public HttpTransport {
 public:
  HttpResponse perform(const HttpRequest& request) override {
    std::lock_guard<std::mutex> lock(mutex_);
    requests.push_back(request);
    if (responses.empty()) return HttpResponse{};  // status 0: simulated network failure.
    HttpResponse response = std::move(responses.front());
    responses.pop_front();
    return response;
  }

  void queue_response(HttpResponse response) {
    std::lock_guard<std::mutex> lock(mutex_);
    responses.push_back(std::move(response));
  }

  std::size_t request_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return requests.size();
  }

  mutable std::mutex mutex_;
  std::deque<HttpResponse> responses;
  std::vector<HttpRequest> requests;
};

// Blocks the test thread until an async Client callback fires, since every
// Client method is asynchronous by design (see licensr.h's class comment
// on Client) and must never be called from the thread it callbacks on.
template <typename T>
class Waiter {
 public:
  std::function<void(licensr::Result<T>)> callback() {
    return [this](licensr::Result<T> r) {
      std::lock_guard<std::mutex> lock(mutex_);
      result_ = std::move(r);
      cv_.notify_all();
    };
  }

  licensr::Result<T> wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    bool completed = cv_.wait_for(lock, std::chrono::seconds(5), [this] { return result_.has_value(); });
    LICENSR_EXPECT_TRUE(completed);
    return std::move(*result_);
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<licensr::Result<T>> result_;
};

licensr::ClientConfig make_config(std::shared_ptr<HttpTransport> transport) {
  licensr::ClientConfig config;
  config.api_key = "pk_test_client_unit_test";
  config.plugin_slug = "unit-test-plugin";
  config.base_url = "https://example.invalid";
  config.transport = std::move(transport);
  // Fast, deterministic backoff so retry tests don't sleep for seconds.
  config.retry.max_retries = 2;
  config.retry.base_delay_ms = 1;
  config.retry.max_delay_ms = 2;
  return config;
}

HttpResponse json_response(int status, std::string body) {
  HttpResponse response;
  response.status = status;
  response.body = std::move(body);
  return response;
}

}  // namespace

LICENSR_TEST(validate_maps_wire_json_to_the_public_result_struct) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(200, R"({
    "valid": true,
    "status": "active",
    "entitlement": "full",
    "feature_flags": {"beta_dsp": true},
    "license": {
      "plan_id": "plan_pro",
      "activation_mode": "seat",
      "max_seats": 5,
      "seats_in_use": 2
    }
  })"));

  licensr::Client client(make_config(transport));
  Waiter<licensr::ValidateResult> waiter;
  client.validate("LIC-KEY", waiter.callback());
  auto result = waiter.wait();

  LICENSR_EXPECT_TRUE(result.ok());
  LICENSR_EXPECT_TRUE(result.value().valid);
  LICENSR_EXPECT_TRUE(result.value().status == licensr::LicenseStatus::kActive);
  LICENSR_EXPECT_EQ(result.value().license.plan_id.value_or(""), std::string("plan_pro"));
  LICENSR_EXPECT_EQ(result.value().license.max_seats.value_or(-1), std::int64_t{5});
  LICENSR_EXPECT_TRUE(result.value().entitlement == licensr::Entitlement::kFull);
  LICENSR_EXPECT_TRUE(result.value().feature_flags.find("beta_dsp") != nullptr);

  // The audio-thread-safe snapshot should reflect the same successful validate.
  auto snapshot = client.snapshot();
  LICENSR_EXPECT_TRUE(snapshot != nullptr);
  LICENSR_EXPECT_TRUE(snapshot->valid);
}

LICENSR_TEST(activate_maps_nested_activation_info) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(200, R"({
    "activation_id": "act-1",
    "feature_flags": {"pro": true},
    "activation": {"type": "domain", "identifier": "example.com"}
  })"));

  licensr::Client client(make_config(transport));
  Waiter<licensr::ActivateResult> waiter;
  licensr::ActivateRequest request;
  request.license_key = "LIC-KEY";
  request.activation_type = licensr::ActivationType::kDomain;
  request.identifier = "example.com";
  client.activate(request, waiter.callback());
  auto result = waiter.wait();

  LICENSR_EXPECT_TRUE(result.ok());
  LICENSR_EXPECT_EQ(result.value().activation_id, std::string("act-1"));
  LICENSR_EXPECT_TRUE(result.value().activation.type == licensr::ActivationType::kDomain);
  LICENSR_EXPECT_EQ(result.value().activation.identifier, std::string("example.com"));
  LICENSR_EXPECT_TRUE(result.value().feature_flags.find("pro") != nullptr);
}

LICENSR_TEST(token_maps_nested_details) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(200, R"({
    "token": "jwt-value",
    "details": {
      "algorithm": "EdDSA",
      "kid": "kid-1",
      "issued_at": "2027-01-01T00:00:00Z",
      "expires_at": "2027-02-01T00:00:00Z",
      "jwks_url": "https://api.example.test/v1/license/jwks/acme/demo.json"
    }
  })"));

  licensr::Client client(make_config(transport));
  Waiter<licensr::TokenResult> waiter;
  client.token("LIC-KEY", waiter.callback());
  auto result = waiter.wait();

  LICENSR_EXPECT_TRUE(result.ok());
  LICENSR_EXPECT_EQ(result.value().token, std::string("jwt-value"));
  LICENSR_EXPECT_EQ(result.value().details.kid, std::string("kid-1"));
  LICENSR_EXPECT_EQ(result.value().details.jwks_url,
                    std::string("https://api.example.test/v1/license/jwks/acme/demo.json"));
}

LICENSR_TEST(checkout_sends_test_clock_only_when_asked_and_maps_test_clock_id) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(200, R"({"checkout_url": "https://pay.example/1", "test_clock_id": null})"));
  transport->queue_response(
      json_response(200, R"({"checkout_url": "https://pay.example/2", "test_clock_id": "clock_123"})"));

  licensr::Client client(make_config(transport));
  licensr::CheckoutRequest request;
  request.plan_id = "plan_pro";
  request.customer_email = "buyer@example.com";

  Waiter<licensr::CheckoutResult> plain;
  client.checkout(request, plain.callback());
  auto plain_result = plain.wait();
  LICENSR_EXPECT_TRUE(plain_result.ok());
  LICENSR_EXPECT_EQ(plain_result.value().checkout_url, std::string("https://pay.example/1"));
  LICENSR_EXPECT_FALSE(plain_result.value().test_clock_id.has_value());

  request.test_clock = true;
  Waiter<licensr::CheckoutResult> clocked;
  client.checkout(request, clocked.callback());
  auto clocked_result = clocked.wait();
  LICENSR_EXPECT_TRUE(clocked_result.ok());
  LICENSR_EXPECT_EQ(clocked_result.value().test_clock_id.value_or(""), std::string("clock_123"));

  std::lock_guard<std::mutex> lock(transport->mutex_);
  LICENSR_EXPECT_EQ(transport->requests.size(), std::size_t{2});
  LICENSR_EXPECT_TRUE(transport->requests[0].body.find("test_clock") == std::string::npos);
  LICENSR_EXPECT_TRUE(transport->requests[1].body.find(R"("test_clock":true)") != std::string::npos);
}

LICENSR_TEST(validate_retries_on_5xx_then_succeeds) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(503, "server unavailable"));
  transport->queue_response(json_response(200, R"({"valid": true, "status": "active"})"));

  licensr::Client client(make_config(transport));
  int retry_count = 0;
  client.events().on_retry = [&](const std::string&, int, int) { ++retry_count; };

  Waiter<licensr::ValidateResult> waiter;
  client.validate("LIC-KEY", waiter.callback());
  auto result = waiter.wait();

  LICENSR_EXPECT_TRUE(result.ok());
  LICENSR_EXPECT_EQ(transport->request_count(), std::size_t{2});
  LICENSR_EXPECT_EQ(retry_count, 1);
}

LICENSR_TEST(validate_surfaces_the_api_error_code_from_the_detail_object) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(
      json_response(403, R"({"detail": {"error": "origin_not_allowed", "message": "Origin not allowed"}})"));

  licensr::Client client(make_config(transport));
  Waiter<licensr::ValidateResult> waiter;
  client.validate("LIC-KEY", waiter.callback());
  auto result = waiter.wait();

  LICENSR_EXPECT_FALSE(result.ok());
  LICENSR_EXPECT_TRUE(result.error().kind == licensr::ErrorKind::kApi);
  LICENSR_EXPECT_EQ(result.error().status, 403);
  LICENSR_EXPECT_EQ(result.error().code, std::string("origin_not_allowed"));
}

LICENSR_TEST(validate_returns_a_network_error_once_retries_are_exhausted) {
  auto transport = std::make_shared<StubHttpTransport>();
  // No responses queued at all: every attempt reports status 0.

  licensr::Client client(make_config(transport));
  Waiter<licensr::ValidateResult> waiter;
  client.validate("LIC-KEY", waiter.callback());
  auto result = waiter.wait();

  LICENSR_EXPECT_FALSE(result.ok());
  LICENSR_EXPECT_TRUE(result.error().kind == licensr::ErrorKind::kNetwork);
  // max_retries=2 means 3 total attempts (the original plus two retries).
  LICENSR_EXPECT_EQ(transport->request_count(), std::size_t{3});
}

LICENSR_TEST(two_clients_for_the_same_installation_share_a_core_but_keep_independent_event_hooks) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(200, R"({"valid": true, "status": "active"})"));

  auto config = make_config(transport);
  licensr::Client client_a(config);
  licensr::Client client_b(config);  // Same api_key/plugin_slug/base_url: shares client_a's ClientCore.

  int a_notifications = 0;
  int b_notifications = 0;
  client_a.events().on_validated = [&](const licensr::ValidateResult&) { ++a_notifications; };
  client_b.events().on_validated = [&](const licensr::ValidateResult&) { ++b_notifications; };

  Waiter<licensr::ValidateResult> waiter;
  client_a.validate("LIC-KEY", waiter.callback());
  waiter.wait();

  // Both listeners are live on the shared core, so a request issued through
  // client_a still notifies client_b's hook — this is the broadcast-registry
  // behavior that replaced a single shared `events_` slot (which would have
  // silently dropped whichever Client was constructed first).
  LICENSR_EXPECT_EQ(a_notifications, 1);
  LICENSR_EXPECT_EQ(b_notifications, 1);
}

LICENSR_TEST(destroying_a_client_unregisters_its_event_hook_from_the_shared_core) {
  auto transport = std::make_shared<StubHttpTransport>();
  transport->queue_response(json_response(200, R"({"valid": true, "status": "active"})"));

  auto config = make_config(transport);
  licensr::Client client_a(config);
  int b_notifications = 0;
  {
    licensr::Client client_b(config);
    client_b.events().on_validated = [&](const licensr::ValidateResult&) { ++b_notifications; };
  }  // client_b destroyed here; its hook must not fire afterwards.

  Waiter<licensr::ValidateResult> waiter;
  client_a.validate("LIC-KEY", waiter.callback());
  waiter.wait();

  LICENSR_EXPECT_EQ(b_notifications, 0);
}
