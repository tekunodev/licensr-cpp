// Process-wide, refcounted "weak singleton" registry: N callers that ask
// for the same Key share one Value instance, and the instance is torn
// down automatically once the last shared_ptr to it goes away. This is
// exactly what a DAW loading N instances of the same plugin needs — one
// worker thread and one heartbeat per installation, not N.
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

namespace licensr::detail {

template <typename Key, typename Value, typename KeyHash>
class RefcountedRegistry {
 public:
  // `factory` is called at most once per key while the registry's lock is
  // held; it must return a `Value*` (typically `new Value(...)`) and must
  // not re-enter `acquire` on the same registry.
  template <typename Factory>
  std::shared_ptr<Value> acquire(const Key& key, Factory factory) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(key);
    if (it != entries_.end()) {
      if (auto existing = it->second.lock()) {
        return existing;
      }
      // Expired weak_ptr left behind by a value mid-destruction; replace it.
      entries_.erase(it);
    }

    std::shared_ptr<Value> value(factory(), [this, key](Value* p) {
      {
        std::lock_guard<std::mutex> erase_lock(mutex_);
        auto entry_it = entries_.find(key);
        if (entry_it != entries_.end() && entry_it->second.expired()) {
          entries_.erase(entry_it);
        }
      }
      delete p;
    });
    entries_.emplace(key, value);
    return value;
  }

  // Test/diagnostic helper: number of currently-live entries.
  std::size_t live_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t count = 0;
    for (const auto& [key, weak] : entries_) {
      if (!weak.expired()) ++count;
    }
    return count;
  }

 private:
  mutable std::mutex mutex_;
  std::unordered_map<Key, std::weak_ptr<Value>, KeyHash> entries_;
};

}  // namespace licensr::detail
