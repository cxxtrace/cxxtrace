#ifndef CXXTRACE_TEST_FOR_EACH_SUBSET_H
#define CXXTRACE_TEST_FOR_EACH_SUBSET_H

#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

namespace cxxtrace_test {
// Enumerate the power set [1] of the given set of items.
//
// callback must have the following signature:
//
//   void(const std::vector<T, Allocator>&)
//
// [1] https://en.wikipedia.org/wiki/Power_set
template<class T, class Allocator, class Func>
auto
for_each_subset(const std::vector<T, Allocator>& items, Func&& callback) -> void
{
  using mask_type = unsigned;
  auto mask_for_item_at_index = [](std::size_t item) -> mask_type {
    assert(item < std::numeric_limits<mask_type>::digits - 1);
    return mask_type{ 1 } << mask_type(item);
  };

  assert(items.size() < std::numeric_limits<mask_type>::digits - 1);

  auto subset_count = mask_for_item_at_index(items.size());
  auto subset = std::vector<T, Allocator>{ items.get_allocator() };
  subset.reserve(items.size());
  for (auto mask = mask_type{ 0 }; mask < subset_count; ++mask) {
    subset.clear();
    for (auto index = mask_type{ 0 }; index < mask_type(items.size());
         ++index) {
      auto item_in_subset = bool(mask & mask_for_item_at_index(index));
      if (item_in_subset) {
        subset.push_back(items[index]);
      }
    }
    callback(subset);
  }
}
}

#endif
