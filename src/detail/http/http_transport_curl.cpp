// Default HTTP transport for Linux (and any other platform where libcurl
// is preferred over the OS-native stack). One curl easy handle per
// request; Licensr issues requests serially from its single worker
// thread, so there's no connection-pooling benefit to a curl multi handle
// or a persistent easy handle here.
#include <curl/curl.h>

#include <cstring>
#include <memory>

#include "licensr/http_transport.h"

namespace {

std::size_t write_body_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* body = static_cast<std::string*>(userdata);
  body->append(ptr, size * nmemb);
  return size * nmemb;
}

std::size_t write_header_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* headers = static_cast<std::vector<std::pair<std::string, std::string>>*>(userdata);
  std::string line(ptr, size * nmemb);
  // Strip the trailing CRLF.
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
  auto colon = line.find(':');
  if (colon == std::string::npos) return size * nmemb;  // Status line or blank separator.
  std::string name = line.substr(0, colon);
  std::string value = line.substr(colon + 1);
  while (!value.empty() && value.front() == ' ') value.erase(value.begin());
  headers->emplace_back(std::move(name), std::move(value));
  return size * nmemb;
}

class CurlHttpTransport : public licensr::HttpTransport {
 public:
  CurlHttpTransport() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlHttpTransport() override { curl_global_cleanup(); }

  licensr::HttpResponse perform(const licensr::HttpRequest& request) override {
    licensr::HttpResponse response;

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
      response.error_message = "curl_easy_init failed";
      return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "licensr-cpp-sdk/1.0 (+https://licensr.app)");

    if (request.timeout_ms > 0) {
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout_ms));
    }

    struct curl_slist* header_list = nullptr;
    for (const auto& [name, value] : request.headers) {
      std::string header_line = name + ": " + value;
      header_list = curl_slist_append(header_list, header_line.c_str());
    }
    if (header_list != nullptr) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    if (request.method == licensr::HttpMethod::kPost) {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    } else {
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
      response.error_message = curl_easy_strerror(rc);
      response.status = 0;
    } else {
      long status = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
      response.status = static_cast<int>(status);
    }

    if (header_list != nullptr) curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return response;
  }
};

}  // namespace

namespace licensr {

std::shared_ptr<HttpTransport> make_default_http_transport() { return std::make_shared<CurlHttpTransport>(); }

}  // namespace licensr
