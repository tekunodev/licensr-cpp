// Exercises compute_hardware_id() against whichever platform implementation
// this test binary was built with (hwid_macos.cpp / hwid_windows.cpp /
// hwid_linux.cpp — see CMakeLists.txt). The raw platform identifier itself
// can't be asserted against a fixed value (it's machine-specific and, by
// design, never observable through this API), so these tests check the
// properties compute_hardware_id() promises instead: determinism,
// salt-sensitivity, and a well-formed SHA-256 hex digest.
#include <string>

#include "licensr/hwid.h"
#include "test_framework.h"

LICENSR_TEST(compute_hardware_id_succeeds_on_a_normal_developer_machine) {
  auto id = licensr::compute_hardware_id("test-salt");
  // Not a hard requirement of the API (see hwid.h: std::nullopt is a valid,
  // documented outcome under restricted permissions or unusual sandboxes),
  // but should hold on the machines this test suite actually runs on.
  LICENSR_EXPECT_TRUE(id.has_value());
  if (!id) return;

  LICENSR_EXPECT_EQ(id->size(), std::size_t{64});  // sha256 hex-encoded.
  for (char c : *id) {
    bool is_lowercase_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    LICENSR_EXPECT_TRUE(is_lowercase_hex);
  }
}

LICENSR_TEST(compute_hardware_id_is_deterministic_for_the_same_salt) {
  auto first = licensr::compute_hardware_id("plugin-a");
  auto second = licensr::compute_hardware_id("plugin-a");
  LICENSR_EXPECT_TRUE(first.has_value());
  LICENSR_EXPECT_TRUE(second.has_value());
  LICENSR_EXPECT_EQ(first.value_or(""), second.value_or(""));
}

LICENSR_TEST(compute_hardware_id_differs_across_salts_on_the_same_machine) {
  // This is the whole point of salting: two unrelated Licensr integrations
  // (or the same plugin's dev vs. prod key) must not produce a
  // correlatable identifier for the same physical machine.
  auto plugin_a = licensr::compute_hardware_id("plugin-a");
  auto plugin_b = licensr::compute_hardware_id("plugin-b");
  LICENSR_EXPECT_TRUE(plugin_a.has_value());
  LICENSR_EXPECT_TRUE(plugin_b.has_value());
  LICENSR_EXPECT_TRUE(plugin_a.value_or("") != plugin_b.value_or(""));
}
