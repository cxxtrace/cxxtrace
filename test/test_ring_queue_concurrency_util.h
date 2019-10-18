#ifndef CXXTRACE_TEST_TEST_RING_QUEUE_CONCURRENCY_UTIL_H
#define CXXTRACE_TEST_TEST_RING_QUEUE_CONCURRENCY_UTIL_H

#include "memory_resource.h"
#include "reduce.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/ring_queue.h>
#include <experimental/memory_resource>
#include <experimental/vector>
#include <optional>
#include <utility>

namespace cxxtrace_test {
template<class Size>
class ring_queue_relacy_test_utilities
{
public:
  using size_type = Size;

  static auto push_back_items_for_range(
    size_type begin,
    size_type end,
    std::experimental::pmr::vector<int>& items) -> void
  {
    items.reserve(items.size() + (end - begin));
    for (auto i = begin; i < end; ++i) {
      items.emplace_back(item_at_index(i));
    }
  }

  static constexpr auto item_at_index(size_type index) noexcept -> int
  {
    return index + 100;
  }
};

// A log of try_push calls to a ring queue.
//
// See pushing_is_atomic_or_fails_with_multiple_producers.
//
// See queue_push_operations<>::possible_outcomes.
template<std::size_t Capacity>
class queue_push_operations
{
public:
  using push_result = cxxtrace::detail::mpsc_ring_queue_push_result;
  using size_type = int;

  static inline constexpr auto capacity = Capacity;
  static inline constexpr auto max_threads = 3;

  explicit queue_push_operations(
    size_type initial_push_size,
    std::array<size_type, max_threads> producer_push_sizes,
    std::array<std::optional<push_result>, max_threads> producer_push_results)
    : initial_push_size{ initial_push_size }
    , producer_push_sizes{ producer_push_sizes }
    , producer_push_results{ producer_push_results }
  {}

  // Return all possible states of the queue after the try_push calls.
  auto possible_outcomes(std::experimental::pmr::memory_resource* memory) const
    -> std::experimental::pmr::vector<std::experimental::pmr::vector<int>>
  {
    using std::experimental::pmr::vector;
    using utilities = ring_queue_relacy_test_utilities<int>;

    auto producer_thread_indexes = vector<int>{ memory };
    for (auto thread_index = 0;
         thread_index < int(producer_push_results.size());
         ++thread_index) {
      if (this->producer_push_results[thread_index] == push_result::pushed) {
        producer_thread_indexes.push_back(thread_index);
      }
    }
    std::sort(producer_thread_indexes.begin(), producer_thread_indexes.end());

    auto expected_items_permutations = vector<vector<int>>{ memory };
    do {
      auto expected_items = vector<int>{ memory };
      utilities::push_back_items_for_range(
        0, this->initial_push_size, expected_items);
      for (auto thread_index : producer_thread_indexes) {
        auto [begin, end] = this->producer_range(thread_index);
        utilities::push_back_items_for_range(begin, end, expected_items);
      }
      if (expected_items.size() > this->capacity) {
        expected_items.erase(expected_items.begin(),
                             expected_items.end() - this->capacity);
      }
      assert(static_cast<size_type>(expected_items.size()) >=
             this->initial_push_size);
      assert(static_cast<size_type>(expected_items.size()) <=
             this->total_push_size());
      expected_items_permutations.emplace_back(std::move(expected_items));
    } while (std::next_permutation(producer_thread_indexes.begin(),
                                   producer_thread_indexes.end()));
    return expected_items_permutations;
  }

private:
  // TODO(strager): Deduplicate with
  // pushing_is_atomic_or_fails_with_multiple_producers.
  auto producer_range(int thread_index) const noexcept
    -> std::pair<size_type, size_type>
  {
    assert(thread_index < int(this->producer_push_sizes.size()));
    auto begin = this->initial_push_size;
    for (auto i = 0; i < thread_index; ++i) {
      begin += this->producer_push_sizes[i];
    }
    auto push_size = this->producer_push_sizes[thread_index];
    return std::pair{ begin, begin + push_size };
  }

  auto total_push_size() const noexcept -> size_type
  {
    return this->initial_push_size + reduce(this->producer_push_sizes.begin(),
                                            this->producer_push_sizes.end());
  }

  size_type initial_push_size;
  std::array<size_type, max_threads> producer_push_sizes;
  std::array<std::optional<push_result>, max_threads> producer_push_results;
};
}

#endif
