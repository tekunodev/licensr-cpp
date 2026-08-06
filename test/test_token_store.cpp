// Exercises FileTokenStore against an explicit `base_dir` override (the
// same hook AUv3 hosts use to point at their own sandboxed container)
// rather than the OS-default app-data directory, so these tests are
// hermetic and never touch a real user profile.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <sstream>

#include "licensr/token_store.h"
#include "test_framework.h"

namespace {

// A fresh temp directory per test, torn down at scope exit.
class ScopedTempDir {
 public:
  ScopedTempDir() {
    std::ostringstream name;
    name << "licensr_test_" << std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / name.str();
  }
  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  std::string string() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

}  // namespace

LICENSR_TEST(save_then_load_round_trips_the_value) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  store.save("LIC-KEY-1", "eyJhbGciOiJFZERTQSJ9.token-body.signature");
  auto loaded = store.load("LIC-KEY-1");

  LICENSR_EXPECT_TRUE(loaded.has_value());
  LICENSR_EXPECT_EQ(*loaded, std::string("eyJhbGciOiJFZERTQSJ9.token-body.signature"));
}

LICENSR_TEST(load_of_a_missing_key_returns_nullopt) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  LICENSR_EXPECT_FALSE(store.load("never-saved").has_value());
}

LICENSR_TEST(distinct_keys_do_not_collide) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  store.save("LIC-KEY-A", "token-a");
  store.save("LIC-KEY-B", "token-b");

  LICENSR_EXPECT_EQ(store.load("LIC-KEY-A").value_or(""), std::string("token-a"));
  LICENSR_EXPECT_EQ(store.load("LIC-KEY-B").value_or(""), std::string("token-b"));
}

LICENSR_TEST(save_overwrites_a_previous_value_for_the_same_key) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  store.save("LIC-KEY-1", "old-token");
  store.save("LIC-KEY-1", "new-token");

  LICENSR_EXPECT_EQ(store.load("LIC-KEY-1").value_or(""), std::string("new-token"));
}

LICENSR_TEST(remove_deletes_the_cached_value) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  store.save("LIC-KEY-1", "token");
  store.remove("LIC-KEY-1");

  LICENSR_EXPECT_FALSE(store.load("LIC-KEY-1").has_value());
}

LICENSR_TEST(remove_of_a_missing_key_is_a_harmless_no_op) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  store.remove("never-saved");  // Must not throw or crash.
  LICENSR_EXPECT_TRUE(true);
}

LICENSR_TEST(the_base_dir_override_is_scoped_by_plugin_slug) {
  ScopedTempDir temp_dir;
  licensr::FileTokenStore store_a("plugin-a", temp_dir.string());
  licensr::FileTokenStore store_b("plugin-b", temp_dir.string());

  store_a.save("SAME-KEY", "belongs-to-a");
  store_b.save("SAME-KEY", "belongs-to-b");

  LICENSR_EXPECT_EQ(store_a.load("SAME-KEY").value_or(""), std::string("belongs-to-a"));
  LICENSR_EXPECT_EQ(store_b.load("SAME-KEY").value_or(""), std::string("belongs-to-b"));
}

LICENSR_TEST(save_creates_the_directory_when_it_does_not_exist_yet) {
  ScopedTempDir temp_dir;
  // Nothing under temp_dir exists yet — save() must create it lazily
  // rather than requiring the caller to mkdir first.
  licensr::FileTokenStore store("unit-test-plugin", temp_dir.string());

  LICENSR_EXPECT_FALSE(std::filesystem::exists(store.directory()));
  store.save("LIC-KEY-1", "token");
  LICENSR_EXPECT_TRUE(std::filesystem::exists(store.directory()));
}
