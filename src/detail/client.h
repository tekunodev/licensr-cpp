// Internal request orchestration shared by every licensr::Client facade
// that points at the same installation (api_key + plugin_slug + base_url).
// See singleton.h for why this is refcounted rather than one-per-Client.
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "detail/json.h"
#include "licensr/licensr.h"

namespace licensr::detail {

class ClientCore {
 public:
  explicit ClientCore(ClientConfig config);
  ~ClientCore();

  ClientCore(const ClientCore&) = delete;
  ClientCore& operator=(const ClientCore&) = delete;

  using Events = Client::Events;

  // Multiple Client facades can share one ClientCore (that's the entire
  // point of the refcounted singleton — see singleton.h), so event hooks
  // are a broadcast registry rather than a single slot: every live
  // registered Events struct is notified of every lifecycle event on this
  // installation, not just the one owned by whichever Client happened to
  // issue a particular request. `add_listener`/`remove_listener` are safe
  // to call from any thread; `remove_listener` is idempotent.
  void add_listener(const std::shared_ptr<Events>& events);
  void remove_listener(const std::shared_ptr<Events>& events);

  void validate(const std::string& license_key, std::function<void(Result<ValidateResult>)> callback);
  void token(const std::string& license_key, std::function<void(Result<TokenResult>)> callback);
  void activate(const ActivateRequest& request, std::function<void(Result<ActivateResult>)> callback);
  void deactivate(const std::string& license_key, const std::string& activation_id,
                  std::function<void(Result<DeactivateResult>)> callback);
  void activations(const std::string& license_key, std::function<void(Result<ActivationsResult>)> callback);
  void checkout(const CheckoutRequest& request, std::function<void(Result<CheckoutResult>)> callback);

  // Atomic snapshot read — safe from the audio thread. Never blocks on the
  // worker thread and never allocates on this path (std::atomic_load on a
  // shared_ptr only bumps a refcount).
  std::shared_ptr<const ValidateResult> snapshot() const;

 private:
  struct PreparedRequest {
    HttpMethod method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> query;
    std::string body_json;  // Empty for GET.
  };

  void enqueue(std::function<void()> task);
  void worker_loop();

  // Performs one logical API call, retrying network errors / 429 / 5xx
  // with exponential backoff + jitter (mirrors sdks/js/src/http.ts
  // exactly). Runs on the worker thread.
  Result<Json> perform_with_retry(const std::string& method_name, const PreparedRequest& request);

  std::vector<std::shared_ptr<Events>> live_listeners() const;
  void emit_retry(const std::string& method_name, int attempt, int delay_ms);
  void emit_error(const std::string& method_name, const Error& error);
  void emit_validated(const ValidateResult& result);
  void emit_activated(const ActivateResult& result);
  void emit_deactivated(const DeactivateResult& result);
  void emit_token_issued(const TokenResult& result);
  void update_snapshot_from_validate(const ValidateResult& result);

  std::string build_url(const std::string& path, const std::vector<std::pair<std::string, std::string>>& query) const;

  ClientConfig config_;
  std::shared_ptr<HttpTransport> transport_;

  std::thread worker_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<std::function<void()>> queue_;
  bool stopping_ = false;

  mutable std::mutex listeners_mutex_;
  std::vector<std::weak_ptr<Events>> listeners_;

  mutable std::shared_ptr<const ValidateResult> snapshot_;
};

// Key identifying one "installation" — a shared worker/heartbeat is only
// safe to reuse across Client instances that would otherwise talk to the
// exact same API key, plugin, and endpoint.
struct ClientCoreKey {
  std::string api_key;
  std::string plugin_slug;
  std::string base_url;

  bool operator==(const ClientCoreKey& other) const {
    return api_key == other.api_key && plugin_slug == other.plugin_slug && base_url == other.base_url;
  }
};

// Returns the process-wide shared ClientCore for `config`'s installation,
// creating it on first use. Every acquire() must be paired with a
// release() (Client's destructor does this) so the last releaser tears
// down the worker thread rather than leaking it for the life of the host
// process.
std::shared_ptr<ClientCore> acquire_client_core(ClientConfig config);

}  // namespace licensr::detail
