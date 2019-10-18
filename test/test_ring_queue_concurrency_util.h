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

  template<class RingQueue>
  static auto push_range(RingQueue& queue,
                         size_type begin,
                         size_type end) noexcept -> void
  {
    for (auto i = begin; i < end; ++i) {
      queue.push(
        1, [&](auto data) noexcept { data.set(0, item_at_index(i)); });
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
private:
  template<class T>
  using vector = std::experimental::pmr::vector<T>;

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
    auto pushes = vector<push>{ memory };
    for (auto thread_index = 0;
         thread_index < int(producer_push_results.size());
         ++thread_index) {
      if (this->producer_push_results[thread_index] == push_result::pushed) {
        pushes.push_back(push{ thread_index });
      }
    }

    auto expected_items_permutations = vector<vector<int>>{ memory };
    enumerate_push_permutations(pushes, [&](vector<int>&& items) -> void {
      expected_items_permutations.emplace_back(std::move(items));
    });
    return expected_items_permutations;
  }

private:
  // A single possibly-executed call to RingQueue::try_push.
  struct push
  {
  private:
    auto members() const noexcept -> auto
    {
      return std::tie(this->thread_index);
    }

  public:
    // The producer which called try_push. The producer_range function
    // determines what items were pushed.
    int thread_index;

    auto operator==(const push& other) const noexcept -> bool
    {
      return this->members() == other.members();
    }

    auto operator!=(const push& other) const noexcept -> bool
    {
      return !(*this == other);
    }

    auto operator<(const push& other) const noexcept -> bool
    {
      return this->members() < other.members();
    }

    auto operator<=(const push& other) const noexcept -> bool
    {
      return this->members() <= other.members();
    }

    auto operator>(const push& other) const noexcept -> bool
    {
      return this->members() > other.members();
    }

    auto operator>=(const push& other) const noexcept -> bool
    {
      return this->members() >= other.members();
    }
  };

  template<class Func>
  auto enumerate_push_permutations(vector<push>& pushes, Func&& callback) const
    -> void
  {
    using utilities = ring_queue_relacy_test_utilities<size_type>;
    std::sort(pushes.begin(), pushes.end());
    do {
      char buffer[1024];
      auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

      auto queue = cxxtrace::detail::ring_queue<int, capacity>{};
      utilities::push_range(queue, 0, this->initial_push_size);
      for (auto& push : pushes) {
        auto [begin, end] = this->producer_range(push.thread_index);
        utilities::push_range(queue, begin, end);
      }

      auto items = vector<int>{ &memory };
      queue.pop_all_into(items);
      assert(static_cast<size_type>(items.size()) >= this->initial_push_size);
      assert(static_cast<size_type>(items.size()) <= this->total_push_size());
      callback(std::move(items));
    } while (std::next_permutation(pushes.begin(), pushes.end()));
  }

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
