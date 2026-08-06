// Default HTTP transport for Windows. Uses WinHTTP rather than WinINet
// (service-safe, no dependency on a logged-in user's IE settings) and
// rather than vendoring/linking OpenSSL or another TLS stack — WinHTTP
// rides the OS's own TLS/certificate store, which is what most DAW hosts'
// own plugins and the OS itself already trust.
#include <windows.h>
#include <winhttp.h>

#include <memory>

#include "licensr/http_transport.h"

#pragma comment(lib, "winhttp.lib")

namespace {

std::wstring to_wide(const std::string& utf8) {
  if (utf8.empty()) return {};
  int required = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), required);
  return wide;
}

std::string to_utf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  int required =
      WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<std::size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), required, nullptr, nullptr);
  return utf8;
}

struct WinHttpHandleDeleter {
  void operator()(HINTERNET handle) const {
    if (handle != nullptr) WinHttpCloseHandle(handle);
  }
};
using WinHttpHandle = std::unique_ptr<void, WinHttpHandleDeleter>;

// Splits a fully-qualified URL into the pieces WinHttpConnect/WinHttpOpenRequest need.
struct ParsedUrl {
  bool https = false;
  std::wstring host;
  INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
  std::wstring path_and_query;
};

bool parse_url(const std::string& url, ParsedUrl* out) {
  std::wstring wide_url = to_wide(url);

  URL_COMPONENTS components{};
  components.dwStructSize = sizeof(components);
  wchar_t host_buffer[256]{};
  wchar_t path_buffer[4096]{};
  components.lpszHostName = host_buffer;
  components.dwHostNameLength = static_cast<DWORD>(std::size(host_buffer));
  components.lpszUrlPath = path_buffer;
  components.dwUrlPathLength = static_cast<DWORD>(std::size(path_buffer));

  if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &components)) return false;

  out->https = components.nScheme == INTERNET_SCHEME_HTTPS;
  out->host.assign(host_buffer);
  out->port = components.nPort;
  out->path_and_query.assign(path_buffer);
  return true;
}

class WinHttpTransport : public licensr::HttpTransport {
 public:
  licensr::HttpResponse perform(const licensr::HttpRequest& request) override {
    licensr::HttpResponse response;

    ParsedUrl parsed;
    if (!parse_url(request.url, &parsed)) {
      response.error_message = "failed to parse URL";
      return response;
    }

    WinHttpHandle session(WinHttpOpen(L"licensr-cpp-sdk/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
      response.error_message = "WinHttpOpen failed";
      return response;
    }

    if (request.timeout_ms > 0) {
      WinHttpSetTimeouts(session.get(), request.timeout_ms, request.timeout_ms, request.timeout_ms, request.timeout_ms);
    }

    WinHttpHandle connect(WinHttpConnect(session.get(), parsed.host.c_str(), parsed.port, 0));
    if (!connect) {
      response.error_message = "WinHttpConnect failed";
      return response;
    }

    const wchar_t* method = request.method == licensr::HttpMethod::kPost ? L"POST" : L"GET";
    DWORD flags = parsed.https ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request_handle(WinHttpOpenRequest(connect.get(), method, parsed.path_and_query.c_str(), nullptr,
                                                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request_handle) {
      response.error_message = "WinHttpOpenRequest failed";
      return response;
    }

    std::wstring header_block;
    for (const auto& [name, value] : request.headers) {
      header_block += to_wide(name) + L": " + to_wide(value) + L"\r\n";
    }
    if (!header_block.empty()) {
      WinHttpAddRequestHeaders(request_handle.get(), header_block.c_str(), static_cast<DWORD>(header_block.size()),
                               WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL sent;
    if (request.method == licensr::HttpMethod::kPost) {
      sent = WinHttpSendRequest(request_handle.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                const_cast<char*>(request.body.data()), static_cast<DWORD>(request.body.size()),
                                static_cast<DWORD>(request.body.size()), 0);
    } else {
      sent =
          WinHttpSendRequest(request_handle.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }
    if (!sent || !WinHttpReceiveResponse(request_handle.get(), nullptr)) {
      response.error_message =
          "WinHttpSendRequest/WinHttpReceiveResponse failed (GLE=" + std::to_string(GetLastError()) + ")";
      return response;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request_handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
                        &status_code, &status_size, nullptr);
    response.status = static_cast<int>(status_code);

    DWORD raw_headers_size = 0;
    WinHttpQueryHeaders(request_handle.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr,
                        &raw_headers_size, nullptr);
    if (raw_headers_size > 0) {
      std::wstring raw_headers(raw_headers_size / sizeof(wchar_t), L'\0');
      if (WinHttpQueryHeaders(request_handle.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                              raw_headers.data(), &raw_headers_size, nullptr)) {
        std::size_t start = 0;
        while (start < raw_headers.size()) {
          std::size_t end = raw_headers.find(L"\r\n", start);
          if (end == std::wstring::npos) break;
          std::wstring line = raw_headers.substr(start, end - start);
          start = end + 2;
          std::size_t colon = line.find(L':');
          if (colon == std::wstring::npos) continue;  // Status line.
          std::wstring name = line.substr(0, colon);
          std::wstring value = line.substr(colon + 1);
          while (!value.empty() && value.front() == L' ') value.erase(value.begin());
          response.headers.emplace_back(to_utf8(name), to_utf8(value));
        }
      }
    }

    std::string body;
    for (;;) {
      DWORD available = 0;
      if (!WinHttpQueryDataAvailable(request_handle.get(), &available) || available == 0) break;
      std::string chunk(available, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(request_handle.get(), chunk.data(), available, &read)) break;
      chunk.resize(read);
      body += chunk;
    }
    response.body = std::move(body);

    return response;
  }
};

}  // namespace

namespace licensr {

std::shared_ptr<HttpTransport> make_default_http_transport() { return std::make_shared<WinHttpTransport>(); }

}  // namespace licensr
