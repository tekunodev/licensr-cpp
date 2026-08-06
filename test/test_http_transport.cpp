#include "licensr/http_transport.h"
#include "test_framework.h"

using licensr::HttpResponse;

LICENSR_TEST(find_header_matches_case_insensitively) {
  HttpResponse response;
  response.headers.emplace_back("Retry-After", "30");
  response.headers.emplace_back("Content-Type", "application/json");

  const std::string* retry_after = response.find_header("retry-after");
  LICENSR_EXPECT_TRUE(retry_after != nullptr);
  LICENSR_EXPECT_EQ(*retry_after, std::string("30"));

  const std::string* content_type = response.find_header("CONTENT-TYPE");
  LICENSR_EXPECT_TRUE(content_type != nullptr);
  LICENSR_EXPECT_EQ(*content_type, std::string("application/json"));
}

LICENSR_TEST(find_header_returns_nullptr_when_absent) {
  HttpResponse response;
  response.headers.emplace_back("Content-Type", "application/json");

  LICENSR_EXPECT_TRUE(response.find_header("X-Missing") == nullptr);
}
