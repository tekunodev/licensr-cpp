#include "detail/singleton.h"
#include "test_framework.h"

namespace {

struct Widget {
  explicit Widget(int id) : id(id) {}
  int id;
};

struct IntHash {
  std::size_t operator()(int key) const { return std::hash<int>{}(key); }
};

using Registry = licensr::detail::RefcountedRegistry<int, Widget, IntHash>;

}  // namespace

LICENSR_TEST(acquire_returns_the_same_instance_for_the_same_key) {
  Registry registry;
  int factory_calls = 0;

  auto a = registry.acquire(1, [&] {
    ++factory_calls;
    return new Widget(1);
  });
  auto b = registry.acquire(1, [&] {
    ++factory_calls;
    return new Widget(1);
  });

  LICENSR_EXPECT_EQ(a.get(), b.get());
  LICENSR_EXPECT_EQ(factory_calls, 1);
  LICENSR_EXPECT_EQ(registry.live_count(), std::size_t{1});
}

LICENSR_TEST(acquire_creates_distinct_instances_for_distinct_keys) {
  Registry registry;

  auto a = registry.acquire(1, [] { return new Widget(1); });
  auto b = registry.acquire(2, [] { return new Widget(2); });

  LICENSR_EXPECT_TRUE(a.get() != b.get());
  LICENSR_EXPECT_EQ(registry.live_count(), std::size_t{2});
}

LICENSR_TEST(entry_is_torn_down_once_the_last_shared_ptr_is_released) {
  Registry registry;

  {
    auto a = registry.acquire(1, [] { return new Widget(1); });
    LICENSR_EXPECT_EQ(registry.live_count(), std::size_t{1});
  }
  LICENSR_EXPECT_EQ(registry.live_count(), std::size_t{0});
}

LICENSR_TEST(acquire_after_teardown_creates_a_fresh_instance) {
  // Deliberately does not compare the two Widget*s for inequality: the
  // allocator is free to reuse the same address once the first instance is
  // deleted, so pointer identity isn't a reliable signal here. The factory
  // call count is what actually proves a second, independent instance was
  // constructed rather than the stale entry being reused.
  Registry registry;
  int factory_calls = 0;
  auto make = [&] {
    ++factory_calls;
    return new Widget(1);
  };

  {
    auto a = registry.acquire(1, make);
  }

  auto b = registry.acquire(1, make);
  LICENSR_EXPECT_EQ(factory_calls, 2);
  LICENSR_EXPECT_EQ(registry.live_count(), std::size_t{1});
}
