#ifndef CXXTRACE_TEST_TEST_RING_QUEUE_CONCURRENCY_UTIL_H
#define CXXTRACE_TEST_TEST_RING_QUEUE_CONCURRENCY_UTIL_H

#include "concatenated_vectors.h"
#include "for_each_subset.h"
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
  enum class push_result
  {
    // try_push was called, but it was interrupted. try_push had no effect on
    // the queue.
    skipped = 0, // Default for value initialization.

    // push or try_push was called and succeeded.
    pushed,

    // try_push was called, but it was interrupted. try_push either had no
    // effect, or it overwrote items in the queue (but did not commit them).
    interrupted,
  };
  using size_type = int;

  static inline constexpr auto capacity = Capacity;
  static inline constexpr auto max_threads = 3;

  explicit queue_push_operations(
    size_type initial_push_size,
    std::array<size_type, max_threads> producer_push_sizes,
    std::array<push_result, max_threads> producer_push_results)
    : initial_push_size{ initial_push_size }
    , producer_push_sizes{ producer_push_sizes }
    , producer_push_results{ producer_push_results }
  {}

  explicit queue_push_operations(
    size_type initial_push_size,
    std::array<size_type, max_threads> producer_push_sizes,
    std::array<std::optional<cxxtrace::detail::mpsc_ring_queue_push_result>,
               max_threads> producer_push_results)
    : initial_push_size{ initial_push_size }
    , producer_push_sizes{ producer_push_sizes }
  {
    for (auto i = std::size_t{ 0 }; i < producer_push_results.size(); ++i) {
      if (producer_push_results[i].has_value()) {
        this->producer_push_results[i] =
          this->convert_push_result(*producer_push_results[i]);
      } else {
        this->producer_push_results[i] = push_result::skipped;
      }
    }
  }

  // Return all possible states of the queue after the try_push calls.
  auto possible_outcomes(std::experimental::pmr::memory_resource* memory) const
    -> std::experimental::pmr::vector<std::experimental::pmr::vector<int>>
  {
    auto required_thread_indexes = vector<int>{ memory };
    auto optional_thread_indexes = vector<int>{ memory };
    for (auto thread_index = 0;
         thread_index < int(this->producer_push_results.size());
         ++thread_index) {
      auto thread_push_result = this->producer_push_results[thread_index];
      switch (thread_push_result) {
        case push_result::pushed:
          required_thread_indexes.push_back(thread_index);
          break;
        case push_result::interrupted:
          optional_thread_indexes.push_back(thread_index);
          break;
        case push_result::skipped:
          break;
      }
    }

    auto outcomes = vector<vector<int>>{ memory };
    auto add_outcome = [&](vector<int>&& expected_items) -> void {
      if (std::find(outcomes.begin(), outcomes.end(), expected_items) ==
          outcomes.end()) {
        outcomes.emplace_back(std::move(expected_items));
      }
    };
    for_each_subset(
      optional_thread_indexes,
      [&](const vector<int>& included_optional_thread_indexes) -> void {
        char buffer[1024];
        auto temporary_memory =
          monotonic_buffer_resource{ buffer, sizeof(buffer) };

        auto thread_indexes =
          concatenated_vectors(required_thread_indexes,
                               included_optional_thread_indexes,
                               &temporary_memory);
        auto pushes = vector<push>{ &temporary_memory };
        pushes.reserve(thread_indexes.size());
        for (auto thread_index : thread_indexes) {
          pushes.push_back(
            push{ thread_index, this->producer_push_results[thread_index] });
        }
        enumerate_push_permutations(pushes, add_outcome);
      });
    return outcomes;
  }

private:
  // A single possibly-executed call to RingQueue::try_push.
  struct push
  {
  private:
    auto members() const noexcept -> auto
    {
      return std::tie(this->thread_index, this->result);
    }

  public:
    // The producer which called try_push. The producer_range function
    // determines what items were pushed.
    int thread_index;

    // Whether the try_push call succeeded, failed and dropped items, or failed
    // and preserved items.
    push_result result;

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
        assert(this->producer_push_results[push.thread_index] == push.result);
        switch (push.result) {
          case push_result::interrupted:
            this->shrink_queue(queue, this->capacity - (end - begin));
            break;
          case push_result::pushed:
            utilities::push_range(queue, begin, end);
            break;
          case push_result::skipped:
            assert(false && "unexpected push result: skipped");
            break;
        }
      }

      auto items = vector<int>{ &memory };
      queue.pop_all_into(items);
      assert(static_cast<size_type>(items.size()) <= this->total_push_size());
      callback(std::move(items));
    } while (std::next_permutation(pushes.begin(), pushes.end()));
  }

  template<class RingQueue>
  static auto shrink_queue(RingQueue& queue,
                           typename RingQueue::size_type max_size) -> void
  {
    using value_type = typename RingQueue::value_type;
    char buffer[RingQueue::capacity * sizeof(value_type)];
    auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

    auto items = vector<value_type>{ &memory };
    queue.pop_all_into(items);
    auto new_item_count = std::min(items.size(), std::size_t(max_size));
    auto begin_index = items.size() - new_item_count;
    for (auto i = begin_index; i < items.size(); ++i) {
      queue.push(1, [&](auto&& writer) { writer.set(0, items.at(i)); });
    }
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

  static auto convert_push_result(
    cxxtrace::detail::mpsc_ring_queue_push_result r) noexcept -> push_result
  {
    using cxxtrace::detail::mpsc_ring_queue_push_result;
    switch (r) {
      case mpsc_ring_queue_push_result::pushed:
        return push_result::pushed;
      case mpsc_ring_queue_push_result::not_pushed_due_to_contention:
        return push_result::skipped;
    }
  }

  size_type initial_push_size;
  std::array<size_type, max_threads> producer_push_sizes;
  std::array<push_result, max_threads> producer_push_results;
};
}

#endif
