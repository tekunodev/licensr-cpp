// Pluggable HTTP transport policy (Cryptolens-style). Licensr ships default
// implementations for WinHTTP (Windows), NSURLSession (macOS/iOS), and
// libcurl (Linux) selected at CMake configure time, but a consumer with its
// own networking stack — a DAW host callback, a game engine's HTTP client —
// can implement this interface instead and pass it via Client::Config.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "licensr/export.h"

namespace licensr {

enum class HttpMethod { kGet, kPost };

struct HttpRequest {
  HttpMethod method = HttpMethod::kGet;
  // Fully-qualified URL, including any query string.
  std::string url;
  // Header name/value pairs, e.g. {"Authorization", "Bearer pk_live_..."}.
  std::vector<std::pair<std::string, std::string>> headers;
  // Empty for GET requests.
  std::string body;
  // 0 means no timeout.
  int timeout_ms = 0;
};

struct HttpResponse {
  // 0 means the request never got a response (DNS/connect/TLS failure,
  // timeout, or the transport was otherwise unable to complete it); in
  // that case `error_message` describes what went wrong.
  int status = 0;
  std::string body;
  // Response header name/value pairs. Names are matched case-insensitively
  // by callers (see `find_header`), so transports may pass through
  // whatever casing the platform networking stack hands back.
  std::vector<std::pair<std::string, std::string>> headers;
  std::string error_message;

  // Case-insensitive header lookup; returns nullptr if absent.
  const std::string* find_header(const std::string& name) const;
};

// `perform` is always called from Licensr's single background worker
// thread, never from the audio thread, and must block until the request
// completes or fails — no callbacks, no async continuations. Keeping the
// transport this simple is what makes it easy to swap in a host's own
// HTTP stack.
class LICENSR_API HttpTransport {
 public:
  virtual ~HttpTransport() = default;
  virtual HttpResponse perform(const HttpRequest& request) = 0;
};

}  // namespace licensr
