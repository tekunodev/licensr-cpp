// Tiny, dependency-free test harness. The SDK core has a zero-third-party-
// dependency goal; the test suite follows the same discipline rather than
// pulling in GoogleTest/Catch2.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace licensr::test {

inline int& failure_count() {
  static int count = 0;
  return count;
}

inline int& test_count() {
  static int count = 0;
  return count;
}

inline void report_failure(const char* file, int line, const std::string& message) {
  failure_count()++;
  std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, message.c_str());
}

}  // namespace licensr::test

#define LICENSR_TEST(name)             \
  static void licensr_test_##name();   \
  namespace {                          \
  struct LicensrTestRegistrar_##name { \
    LicensrTestRegistrar_##name() {    \
      ::licensr::test::test_count()++; \
      licensr_test_##name();           \
    }                                  \
  } licensr_test_registrar_##name;     \
  }                                    \
  static void licensr_test_##name()

#define LICENSR_EXPECT_TRUE(cond)                                                   \
  do {                                                                              \
    if (!(cond)) {                                                                  \
      ::licensr::test::report_failure(__FILE__, __LINE__, "expected true: " #cond); \
    }                                                                               \
  } while (0)

#define LICENSR_EXPECT_FALSE(cond)                                                   \
  do {                                                                               \
    if (cond) {                                                                      \
      ::licensr::test::report_failure(__FILE__, __LINE__, "expected false: " #cond); \
    }                                                                                \
  } while (0)

#define LICENSR_EXPECT_EQ(a, b)                                                                         \
  do {                                                                                                  \
    if (!((a) == (b))) {                                                                                \
      ::licensr::test::report_failure(__FILE__, __LINE__, std::string("expected ") + #a + " == " + #b); \
    }                                                                                                   \
  } while (0)

#define LICENSR_TEST_MAIN()                                                                     \
  int main() {                                                                                  \
    std::fprintf(stdout, "%d test case(s) ran, %d failure(s)\n", ::licensr::test::test_count(), \
                 ::licensr::test::failure_count());                                             \
    return ::licensr::test::failure_count() == 0 ? 0 : 1;                                       \
  }
