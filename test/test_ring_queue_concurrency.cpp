#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "memory_resource.h"
#include "ring_queue_wrapper.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/mpmc_ring_queue.h>
#include <cxxtrace/detail/spmc_ring_queue.h>
#include <cxxtrace/string.h>
#include <experimental/memory_resource>
#include <experimental/vector>
#include <numeric>
#include <optional>
#include <ostream>
#include <type_traits> // IWYU pragma: keep
#include <utility>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <relacy/stdlib/../context_base.hpp>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <vector>

#if defined(__GLIBCXX__)
// libstdc++ version 9.1.0 does not define some overloads of
// std::reduce.
#define CXXTRACE_WORK_AROUND_STD_REDUCE 1
#endif

#if CXXTRACE_WORK_AROUND_STD_REDUCE
#include <iterator>
#endif

namespace {
auto
assert_items_are_sequential(const std::experimental::pmr::vector<int>& items)
  -> void;

template<class Iterator>
auto
reduce(Iterator begin, Iterator end) -> auto
{
#if CXXTRACE_WORK_AROUND_STD_REDUCE
  return std::accumulate(
    begin, end, typename std::iterator_traits<Iterator>::value_type());
#else
  return std::reduce(begin, end);
#endif
}
}

namespace cxxtrace {
namespace detail {
auto
operator<<(std::ostream& out, mpmc_ring_queue_push_result x) -> std::ostream&
{
  switch (x) {
    case mpmc_ring_queue_push_result::pushed:
      out << "pushed";
      break;
    case mpmc_ring_queue_push_result::not_pushed_due_to_contention:
      out << "not_pushed_due_to_contention";
      break;
  }
  return out;
}
}
}

namespace cxxtrace_test {
template<class RingQueue>
class ring_queue_relacy_test_base
{
protected:
  using size_type = typename RingQueue::size_type;

  static_assert(std::is_same_v<typename RingQueue::value_type, int>);

  static constexpr auto capacity = RingQueue::capacity;

  auto push_range(size_type begin, size_type end) noexcept -> void
  {
    for (auto i = begin; i < end; ++i) {
      this->queue.push(
        1, [i](auto data) noexcept { data.set(0, item_at_index(i)); });
    }
  }

  auto bulk_push_range(size_type begin, size_type end) noexcept -> void
  {
    this->queue.push(
      end - begin, [&](auto data) noexcept {
        this->write_range(begin, end, data);
      });
  }

  template<class RQ = RingQueue>
  auto try_bulk_push_range(size_type begin, size_type end) noexcept ->
    typename RQ::push_result
  {
    return this->queue.try_push(
      end - begin, [&](auto data) noexcept {
        this->write_range(begin, end, data);
      });
  }

  template<class WriteHandle>
  static auto write_range(size_type begin,
                          size_type end,
                          WriteHandle& data) noexcept -> void
  {
    for (auto i = begin; i < end; ++i) {
      // Hunt for bugs. If the reader thread sees these intermediate values,
      // assert_items_are_sequential will fail.
      data.set(i - begin, 9999);
    }
    for (auto i = begin; i < end; ++i) {
      data.set(i - begin, item_at_index(i));
    }
  }

  auto items_for_range(size_type begin,
                       size_type end,
                       std::experimental::pmr::memory_resource* memory) const
    -> std::experimental::pmr::vector<int>
  {
    auto items = std::experimental::pmr::vector<int>{ memory };
    items.reserve(end - begin);
    for (auto i = begin; i < end; ++i) {
      items.emplace_back(this->item_at_index(i));
    }
    return items;
  }

  auto push_back_items_for_range(
    size_type begin,
    size_type end,
    std::experimental::pmr::vector<int>& items) const -> void
  {
    items.reserve(items.size() + (end - begin));
    for (auto i = begin; i < end; ++i) {
      items.emplace_back(this->item_at_index(i));
    }
  }

  static constexpr auto item_at_index(size_type index) noexcept -> int
  {
    return index + 100;
  }

  static auto log_items(cxxtrace::czstring name,
                        const std::experimental::pmr::vector<int>& items,
                        cxxtrace::detail::debug_source_location caller) -> void
  {
    concurrency_log(
      [&](std::ostream& out) {
        out << name << ": {";
        auto need_comma = false;
        for (auto item : items) {
          if (need_comma) {
            out << ", ";
          }
          out << item;
          need_comma = true;
        }
        out << "}";
      },
      caller);
  }

  ring_queue_wrapper<RingQueue> queue{};
};

template<class RingQueue>
class ring_queue_push_one_pop_all_relacy_test
  : public ring_queue_relacy_test_base<RingQueue>
{
public:
  auto run_thread(int thread_index) -> void
  {
    if (thread_index == 0) {
      this->queue.push(
        1, [](auto data) noexcept { data.set(0, 42); });
    } else {
      char buffer[1024];
      auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

      auto items = std::experimental::pmr::vector<int>{ &memory };
      this->queue.pop_all_into(items);
      CXXTRACE_ASSERT(items.size() == 0 || items.size() == 1);
      if (items.size() == 1) {
        CXXTRACE_ASSERT(items[0] == 42);
      }
    }
  }

  auto tear_down() -> void {}
};

enum class ring_queue_push_kind
{
  one_push_per_item,
  single_bulk_push,
};

template<class RingQueue>
class ring_queue_overflow_drops_some_but_not_all_items_relacy_test
  : public ring_queue_relacy_test_base<RingQueue>
{
public:
  using size_type = typename RingQueue::size_type;

  explicit ring_queue_overflow_drops_some_but_not_all_items_relacy_test(
    size_type initial_overflow,
    size_type concurrent_overflow,
    ring_queue_push_kind push_kind) noexcept
    : initial_overflow{ initial_overflow }
    , concurrent_overflow{ concurrent_overflow }
    , push_kind{ push_kind }
  {
    this->push_range(0, this->capacity + this->initial_overflow);
  }

  auto run_thread(int thread_index) -> void
  {
    if (thread_index == 0) {
      auto begin = this->capacity + this->initial_overflow;
      auto end = begin + this->concurrent_overflow;
      switch (this->push_kind) {
        case ring_queue_push_kind::one_push_per_item:
          this->push_range(begin, end);
          break;
        case ring_queue_push_kind::single_bulk_push:
          this->bulk_push_range(begin, end);
          break;
      }
    } else {
      char buffer[1024];
      auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

      auto items = std::experimental::pmr::vector<int>{ &memory };
      this->queue.pop_all_into(items);
      CXXTRACE_ASSERT(static_cast<size_type>(items.size()) <= this->capacity);
      CXXTRACE_ASSERT(static_cast<size_type>(items.size()) >=
                      this->capacity - this->concurrent_overflow);
      assert_items_are_sequential(items);
    }
  }

  auto tear_down() -> void {}

private:
  size_type initial_overflow;
  size_type concurrent_overflow;
  ring_queue_push_kind push_kind;
};

template<class RingQueue>
class ring_queue_pop_eventually_returns_last_item_relacy_test
  : public ring_queue_relacy_test_base<RingQueue>
{
public:
  using size_type = typename RingQueue::size_type;

  explicit ring_queue_pop_eventually_returns_last_item_relacy_test(
    size_type initial_push_size,
    size_type concurrent_push_size) noexcept
    : initial_push_size{ initial_push_size }
    , concurrent_push_size{ concurrent_push_size }
  {
    this->push_range(0, this->initial_push_size);
    assert(this->total_push_size() > 0);
  }

  auto run_thread(int thread_index) -> void
  {
    if (thread_index == 0) {
      this->push_range(this->initial_push_size, this->total_push_size());
    } else {
      auto found_last_item = false;
      while (!found_last_item) {
        char buffer[1024];
        auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

        auto items = std::experimental::pmr::vector<int>{ &memory };
        auto backoff = cxxtrace_test::backoff{};
        this->queue.pop_all_into(items);
        while (items.empty()) {
          backoff.yield(CXXTRACE_HERE);
          this->queue.pop_all_into(items);
        }

        if (items.back() == this->expected_last_item()) {
          found_last_item = true;
        }
      }

      {
        char buffer[1024];
        auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

        auto items = std::experimental::pmr::vector<int>{};
        this->queue.pop_all_into(items);
        CXXTRACE_ASSERT(items.empty());
      }
    }
  }

  auto tear_down() -> void {}

private:
  auto total_push_size() const noexcept -> size_type
  {
    return this->initial_push_size + this->concurrent_push_size;
  }

  auto expected_last_item() const noexcept
  {
    return this->item_at_index(this->total_push_size() - 1);
  }

  size_type initial_push_size;
  size_type concurrent_push_size;
};

template<class RingQueue>
class ring_queue_pop_reads_all_items_relacy_test
  : public ring_queue_relacy_test_base<RingQueue>
{
public:
  using size_type = typename RingQueue::size_type;

  explicit ring_queue_pop_reads_all_items_relacy_test(
    size_type initial_push_size,
    size_type concurrent_push_size) noexcept
    : initial_push_size{ initial_push_size }
    , concurrent_push_size{ concurrent_push_size }
  {
    this->push_range(0, this->initial_push_size);
    assert(this->total_push_size() > 0);
    assert(this->total_push_size() <= RingQueue::capacity);
  }

  auto run_thread(int thread_index) -> void
  {
    if (thread_index == 0) {
      this->push_range(this->initial_push_size, this->total_push_size());
    } else {
      char buffer[1024];
      auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

      auto items = std::experimental::pmr::vector<int>{ &memory };
      while (items.size() < static_cast<std::size_t>(this->total_push_size())) {
        auto old_size = items.size();
        auto backoff = cxxtrace_test::backoff{};
        this->queue.pop_all_into(items);
        while (items.size() == old_size) {
          backoff.yield(CXXTRACE_HERE);
          this->queue.pop_all_into(items);
        }
      }
      CXXTRACE_ASSERT(items == this->expected_items(&memory));
    }
  }

  auto tear_down() -> void {}

private:
  auto total_push_size() const noexcept -> size_type
  {
    return this->initial_push_size + this->concurrent_push_size;
  }

  auto expected_items(std::experimental::pmr::memory_resource* memory) const
    -> std::experimental::pmr::vector<int>
  {
    return this->items_for_range(0, this->total_push_size(), memory);
  }

  size_type initial_push_size;
  size_type concurrent_push_size;
};

template<class RingQueue>
class ring_queue_concurrent_pops_read_all_items_exactly_once_relacy_test
  : public ring_queue_relacy_test_base<RingQueue>
{
public:
  using size_type = typename RingQueue::size_type;

  explicit ring_queue_concurrent_pops_read_all_items_exactly_once_relacy_test(
    size_type initial_push_size,
    size_type concurrent_push_size,
    int thread_count) noexcept
    : initial_push_size{ initial_push_size }
    , concurrent_push_size{ concurrent_push_size }
    , items_popped_per_thread{ &this->memory }
  {
    this->push_range(0, this->initial_push_size);
    assert(this->total_push_size() <= RingQueue::capacity);

    this->items_popped_per_thread.reserve(thread_count);
    for (auto i = 0; i < thread_count; ++i) {
      auto& thread_items = this->items_popped_per_thread.emplace_back(
        std::experimental::pmr::vector<int>{ &this->memory });
      thread_items.reserve(this->total_push_size());
    }
  }

  auto run_thread(int thread_index) -> void
  {
    if (thread_index == 0) {
      this->push_range(this->initial_push_size, this->total_push_size());
    } else {
      char buffer[1024];
      auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

      auto thread_items = std::experimental::pmr::vector<int>{ &memory };
      auto backoff = cxxtrace_test::backoff{};
      while (this->popped_item_count.load(CXXTRACE_HERE) <
             this->total_push_size()) {
        auto old_size = thread_items.size();
        this->queue.pop_all_into(thread_items);
        auto just_popped_item_count = thread_items.size() - old_size;
        if (just_popped_item_count == 0) {
          backoff.yield(CXXTRACE_HERE);
        } else {
          this->popped_item_count.fetch_add(just_popped_item_count,
                                            CXXTRACE_HERE);
          backoff.reset();
        }
      }
      CXXTRACE_ASSERT(thread_items.size() <=
                      static_cast<std::size_t>(this->total_push_size()));
      if (thread_items.size() <=
          static_cast<std::size_t>(this->total_push_size())) {
        // N.B. Avoid allocating when moving into this->items_popped_per_thread.
        // Its allocator is not thread-safe.
        auto& shared_thread_items = this->items_popped_per_thread[thread_index];
        assert(shared_thread_items.empty());
        assert(shared_thread_items.capacity() >= thread_items.size());
        shared_thread_items.insert(
          shared_thread_items.end(), thread_items.begin(), thread_items.end());
      }
    }
  }

  auto tear_down() -> void
  {
    char buffer[1024];
    auto memory = monotonic_buffer_resource{ buffer, sizeof(buffer) };

    auto expected_items = this->expected_items(&memory);
    assert(std::is_sorted(expected_items.begin(), expected_items.end()));

    auto all_popped_items = std::experimental::pmr::vector<int>{ &memory };
    all_popped_items.reserve(expected_items.size());
    for (const auto& thread_items : this->items_popped_per_thread) {
      CXXTRACE_ASSERT(std::is_sorted(thread_items.begin(), thread_items.end()));
      all_popped_items.insert(
        all_popped_items.end(), thread_items.begin(), thread_items.end());
    }
    std::sort(all_popped_items.begin(), all_popped_items.end());
    CXXTRACE_ASSERT(all_popped_items == expected_items);
  }

private:
  auto total_push_size() const noexcept -> size_type
  {
    return this->initial_push_size + this->concurrent_push_size;
  }

  auto expected_items(std::experimental::pmr::memory_resource* memory) const
    -> std::experimental::pmr::vector<int>
  {
    return this->items_for_range(0, this->total_push_size(), memory);
  }

  // HACK(strager): Model checkers want to think that popped_item_count
  // is an atomic variable under test. This isn't true, and if we do nothing,
  // execution time suffers. Try to hide popped_item_count from the checkers.
#if CXXTRACE_ENABLE_CDSCHECKER
  // HACK(strager): CDSChecker shadows std::atomic anyway, so use our wrapper.
  // (Our wrapper works around bugs in CDSChecker's implementation of
  // std::atomic.)
  template<class T>
  using atomic = cxxtrace::detail::cdschecker_atomic<T>;
#else
  template<class T>
  using atomic = cxxtrace::detail::real_atomic<T>;
#endif

  size_type initial_push_size;
  size_type concurrent_push_size;

  atomic<int> popped_item_count{ 0 };

  char buffer[1024];
  monotonic_buffer_resource memory{ this->buffer, sizeof(this->buffer) };
  std::experimental::pmr::vector<std::experimental::pmr::vector<int>>
    items_popped_per_thread;
};

template<class RingQueue>
class pushing_is_atomic_or_fails_with_multiple_producers
  : public ring_queue_relacy_test_base<RingQueue>
{
public:
  using size_type = typename RingQueue::size_type;

  explicit pushing_is_atomic_or_fails_with_multiple_producers(
    size_type initial_push_size,
    size_type producer_1_push_size,
    size_type producer_2_push_size)
    : pushing_is_atomic_or_fails_with_multiple_producers{ initial_push_size,
                                                          producer_1_push_size,
                                                          producer_2_push_size,
                                                          0 }
  {}

  explicit pushing_is_atomic_or_fails_with_multiple_producers(
    size_type initial_push_size,
    size_type producer_1_push_size,
    size_type producer_2_push_size,
    size_type producer_3_push_size)
    : initial_push_size{ initial_push_size }
    , producer_push_sizes{ producer_1_push_size,
                           producer_2_push_size,
                           producer_3_push_size }
  {
    if (this->initial_push_size > 0) {
      auto result = this->try_bulk_push_range(0, this->initial_push_size);
      CXXTRACE_ASSERT(result == push_result::pushed);
    }
  }

  auto run_thread(int thread_index) -> void
  {
    assert(thread_index < int(this->producer_push_results.size()));

    auto [begin, end] = this->producer_range(thread_index);
    auto push_result = this->try_bulk_push_range(begin, end);
    this->producer_push_results[thread_index] = push_result;
    concurrency_log(
      [&](std::ostream& out) {
        out << "producer_push_results[" << thread_index
            << "] = " << push_result;
      },
      CXXTRACE_HERE);
  }

  auto tear_down() -> void
  {
    using std::experimental::pmr::vector;
    auto buffer = std::array<std::byte, 1024>{};
    auto memory = monotonic_buffer_resource{ buffer.data(), buffer.size() };

    CXXTRACE_ASSERT(std::any_of(
      this->producer_push_results.begin(),
      this->producer_push_results.end(),
      [](std::optional<push_result> r) { return r == push_result::pushed; }));

    auto expected_items_permutations =
      this->expected_items_permutations(&memory);
    assert(!expected_items_permutations.empty());
    for (const auto& expected_items : expected_items_permutations) {
      this->log_items(
        "expected items possibility", expected_items, CXXTRACE_HERE);
    }

    auto items = vector<int>{ &memory };
    this->queue.pop_all_into(items);
    this->log_items("items", items, CXXTRACE_HERE);
    CXXTRACE_ASSERT(std::any_of(expected_items_permutations.begin(),
                                expected_items_permutations.end(),
                                [&](const vector<int>& expected_items) {
                                  return expected_items == items;
                                }));
  }

private:
  using push_result = typename RingQueue::push_result;

  auto total_push_size() const noexcept -> size_type
  {
    return this->initial_push_size + reduce(this->producer_push_sizes.begin(),
                                            this->producer_push_sizes.end());
  }

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

  auto expected_items_permutations(
    std::experimental::pmr::memory_resource* memory)
    -> std::experimental::pmr::vector<std::experimental::pmr::vector<int>>
  {
    using std::experimental::pmr::vector;

    auto producer_thread_indexes = vector<int>{};
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
      this->push_back_items_for_range(
        0, this->initial_push_size, expected_items);
      for (auto thread_index : producer_thread_indexes) {
        auto [begin, end] = this->producer_range(thread_index);
        this->push_back_items_for_range(begin, end, expected_items);
      }
      if (expected_items.size() > RingQueue::capacity) {
        expected_items.erase(expected_items.begin(),
                             expected_items.end() - RingQueue::capacity);
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

  static inline constexpr auto max_threads = 3;

  size_type initial_push_size;
  std::array<size_type, max_threads> producer_push_sizes;

  std::array<std::optional<push_result>, max_threads> producer_push_results;
};

template<template<class T, std::size_t Capacity, class Index = int>
         class RingQueue>
auto
register_single_producer_ring_queue_concurrency_tests() -> void
{
  register_concurrency_test<
    ring_queue_push_one_pop_all_relacy_test<RingQueue<int, 64>>>(
    2, concurrency_test_depth::full);

  for (auto push_kind : { ring_queue_push_kind::one_push_per_item,
                          ring_queue_push_kind::single_bulk_push }) {
    for (auto initial_overflow : { 0, 2, 5 }) {
      auto concurrent_overflow = 2;
      register_concurrency_test<
        ring_queue_overflow_drops_some_but_not_all_items_relacy_test<
          RingQueue<int, 4>>>(2,
                              concurrency_test_depth::full,
                              initial_overflow,
                              concurrent_overflow,
                              push_kind);
    }
  }

  for (auto initial_push_size : { 0, 3 }) {
    for (auto concurrent_push_size : { 1, 2 }) {
      register_concurrency_test<
        ring_queue_pop_eventually_returns_last_item_relacy_test<
          RingQueue<int, 4>>>(2,
                              concurrency_test_depth::shallow,
                              initial_push_size,
                              concurrent_push_size);
    }
  }

  for (auto [initial_push_size, concurrent_push_size] :
       { std::pair{ 0, 1 }, std::pair{ 3, 1 }, std::pair{ 0, 2 } }) {
    register_concurrency_test<
      ring_queue_pop_reads_all_items_relacy_test<RingQueue<int, 4>>>(
      2,
      concurrency_test_depth::shallow,
      initial_push_size,
      concurrent_push_size);
  }

  for (auto [initial_push_size, concurrent_push_size] : { std::pair{ 1, 0 },
                                                          std::pair{ 2, 0 },
                                                          std::pair{ 0, 1 },
                                                          std::pair{ 3, 1 } }) {
    auto thread_count = 3;
    // TODO(strager): Optimize this test and check with concurrent_push_size>=2.
    register_concurrency_test<
      ring_queue_concurrent_pops_read_all_items_exactly_once_relacy_test<
        RingQueue<int, 4>>>(thread_count,
                            concurrency_test_depth::shallow,
                            initial_push_size,
                            concurrent_push_size,
                            thread_count);
  }
}

template<template<class T, std::size_t Capacity, class Index = int>
         class RingQueue>
auto
register_multiple_producer_ring_queue_concurrency_tests() -> void
{
  {
    auto initial_push_size = 0;
    auto producer_1_push_size = 1;
    for (auto producer_2_push_size : { 1, 2 }) {
      register_concurrency_test<
        pushing_is_atomic_or_fails_with_multiple_producers<RingQueue<int, 4>>>(
        2,
        concurrency_test_depth::full,
        initial_push_size,
        producer_1_push_size,
        producer_2_push_size);
    }
  }

  {
    auto initial_push_size = 0;
    auto producer_1_push_size = 1;
    auto producer_2_push_size = 1;
    auto producer_3_push_size = 1;
    register_concurrency_test<
      pushing_is_atomic_or_fails_with_multiple_producers<RingQueue<int, 4>>>(
      3,
      concurrency_test_depth::full,
      initial_push_size,
      producer_1_push_size,
      producer_2_push_size,
      producer_3_push_size);
  }

  {
    constexpr auto initial_push_size = 0;
    constexpr auto producer_1_push_size = 3;
    constexpr auto producer_2_push_size = 3;
    constexpr auto size = 4;
    static_assert(producer_1_push_size + producer_2_push_size > size);
    register_concurrency_test<
      pushing_is_atomic_or_fails_with_multiple_producers<RingQueue<int, size>>>(
      2,
      concurrency_test_depth::full,
      initial_push_size,
      producer_1_push_size,
      producer_2_push_size);
  }
}

auto
register_concurrency_tests() -> void
{
  register_single_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::spmc_ring_queue>();
  register_single_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::mpmc_ring_queue>();
  register_multiple_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::mpmc_ring_queue>();
}
}

namespace {
auto
assert_items_are_sequential(const std::experimental::pmr::vector<int>& items)
  -> void
{
  if (!items.empty()) {
    for (auto i = 1; i < int(items.size()); ++i) {
      CXXTRACE_ASSERT(items[i] == items[i - 1] + 1);
    }
  }
}
}
