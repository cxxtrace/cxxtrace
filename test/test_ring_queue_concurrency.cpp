#if !(CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_CONCURRENCY_STRESS ||      \
      CXXTRACE_ENABLE_RELACY)
#define CXXTRACE_ENABLE_GTEST 1
#endif

#if !CXXTRACE_ENABLE_GTEST
#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "rseq_scheduler.h"
#include <cxxtrace/detail/processor_local_mpsc_ring_queue.h>
#include <cxxtrace/detail/spsc_ring_queue.h>
#endif

#include "memory_resource.h"
#include "ring_queue_wrapper.h"
#include "synchronization.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/string.h>
#include <experimental/memory_resource>
#include <experimental/vector>
#include <numeric>
#include <optional>
#include <ostream>
#include <type_traits> // IWYU pragma: keep
#include <utility>
// IWYU pragma: no_include "relacy_backoff.h"
// IWYU pragma: no_include "relacy_synchronization.h"
// IWYU pragma: no_include <cxxtrace/detail/real_synchronization.h>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <relacy/stdlib/../context_base.hpp>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <vector>

#if CXXTRACE_ENABLE_GTEST
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#endif

#if defined(__GLIBCXX__)
// libstdc++ version 9.1.0 does not define some overloads of
// std::reduce.
#define CXXTRACE_WORK_AROUND_STD_REDUCE 1
#endif

#if CXXTRACE_WORK_AROUND_STD_REDUCE
#include <iterator>
#endif

#if CXXTRACE_ENABLE_GTEST
#define CXXTRACE_ASSERT(condition) EXPECT_TRUE((condition))
#endif

#if CXXTRACE_ENABLE_GTEST
using testing::UnorderedElementsAre;
#endif

namespace {
#if !CXXTRACE_ENABLE_GTEST
auto
assert_items_are_sequential(const std::experimental::pmr::vector<int>& items)
  -> void;
#endif

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

// Enumerate the power set [1] of the given set of items.
//
// callback must have the following signature:
//
//   void(const std::vector<T, Allocator>&)
//
// [1] https://en.wikipedia.org/wiki/Power_set
template<class T, class Allocator, class Func>
auto
for_each_subset(const std::vector<T, Allocator>& items, Func&& callback)
  -> void;
}

namespace cxxtrace {
namespace detail {
auto
operator<<(std::ostream& out, mpsc_ring_queue_push_result x) -> std::ostream&
{
  switch (x) {
    case mpsc_ring_queue_push_result::pushed:
      out << "pushed";
      break;
    case mpsc_ring_queue_push_result::not_pushed_due_to_contention:
      out << "not_pushed_due_to_contention";
      break;
    case mpsc_ring_queue_push_result::push_interrupted_due_to_preemption:
      out << "push_interrupted_due_to_preemption";
      break;
  }
  return out;
}
}
}

namespace cxxtrace_test {
using sync = concurrency_test_synchronization;

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
                        sync::debug_source_location caller) -> void
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

#if !CXXTRACE_ENABLE_GTEST
  rseq_scheduler<sync> rseq{};
#endif
};

#if !CXXTRACE_ENABLE_GTEST
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
        auto backoff = sync::backoff{};
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
        auto backoff = sync::backoff{};
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
#endif

// @@@ this test name is wrong for processor_local_mpsc_ring_queue.
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

    if (!std::any_of(this->producer_push_results.begin(),
                     this->producer_push_results.end(),
                     [](std::optional<push_result> r) {
                       return r == push_result::pushed;
                     })) {
      return; // @@@ we should at least assert that we pushed nothing.
    }

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

public:
  auto expected_items_permutations(
    std::experimental::pmr::memory_resource* memory)
    -> std::experimental::pmr::vector<std::experimental::pmr::vector<int>>
  {
    using std::experimental::pmr::vector;

    auto shrink = [](auto& items, int /*@@@ use typedef*/ max_size) -> void {
      if (items.size() > std::size_t(max_size)) {
        items.erase(items.begin(), items.end() - max_size);
      }
    };

    auto expected_items_permutations = vector<vector<int>>{ memory };
    auto collect_expected_item_permutations =
      [&](vector<int>& producer_thread_indexes) -> void {
      do {
        auto expected_items = vector<int>{ memory };
        this->push_back_items_for_range(
          0, this->initial_push_size, expected_items);
        for (auto thread_index : producer_thread_indexes) {
          auto [begin, end] = this->producer_range(thread_index);
          // @@@ convert to switch?
          if (this->producer_push_results[thread_index] ==
              push_result::push_interrupted_due_to_preemption) {
            shrink(expected_items, RingQueue::capacity - (end - begin));
          } else {
            this->push_back_items_for_range(begin, end, expected_items);
          }
        }
        shrink(expected_items, RingQueue::capacity);
        assert(static_cast<size_type>(expected_items.size()) <=
               this->total_push_size());
        expected_items_permutations.emplace_back(std::move(expected_items));
      } while (std::next_permutation(producer_thread_indexes.begin(),
                                     producer_thread_indexes.end()));
    };

    auto pushed_producer_thread_indexes = vector<int>{ memory };
    auto interrupted_producer_thread_indexes = vector<int>{ memory };
    for (auto thread_index = 0;
         thread_index < int(producer_push_results.size());
         ++thread_index) {
      auto thread_push_result = this->producer_push_results[thread_index];
      if (!thread_push_result.has_value()) {
        continue;
      }
      switch (*thread_push_result) {
        case push_result::pushed:
          pushed_producer_thread_indexes.push_back(thread_index);
          break;
        case push_result::push_interrupted_due_to_preemption:
          interrupted_producer_thread_indexes.push_back(thread_index);
          break;
        case push_result::not_pushed_due_to_contention:
          break;
      }
    }

    // @@@ these variable names are very long. shorten.
    for_each_subset(
      interrupted_producer_thread_indexes,
      [&](const vector<int>& current_interrupted_producer_thread_indexes)
        -> void {
        auto producer_thread_indexes =
          vector<int>{ pushed_producer_thread_indexes.begin(),
                       pushed_producer_thread_indexes.end(),
                       memory };
        producer_thread_indexes.insert(
          producer_thread_indexes.end(),
          current_interrupted_producer_thread_indexes.begin(),
          current_interrupted_producer_thread_indexes.end());
        std::sort(producer_thread_indexes.begin(),
                  producer_thread_indexes.end());
        collect_expected_item_permutations(producer_thread_indexes);
      });

    // @@@ put into a function: sort_and_remove_duplicates
    std::sort(expected_items_permutations.begin(),
              expected_items_permutations.end());
    expected_items_permutations.erase(
      std::unique(expected_items_permutations.begin(),
                  expected_items_permutations.end()),
      expected_items_permutations.end());

    return expected_items_permutations;
  }

private:
  static inline constexpr auto max_threads = 3;

  size_type initial_push_size;
  std::array<size_type, max_threads> producer_push_sizes;

public:
  std::array<std::optional<push_result>, max_threads> producer_push_results;
};

#if CXXTRACE_ENABLE_GTEST
class test_pushing_is_atomic_or_fails : public testing::Test
{
protected:
  template<class T>
  using vector = std::experimental::pmr::vector<T>;
  using ring_queue_type = cxxtrace::detail::mpsc_ring_queue<int, 4>;
  using test_type =
    pushing_is_atomic_or_fails_with_multiple_producers<ring_queue_type>;
  using push_result = ring_queue_type::push_result;

  static inline std::experimental::pmr::memory_resource* memory =
    std::experimental::pmr::new_delete_resource();
};

TEST_F(test_pushing_is_atomic_or_fails, two_successful_pushes_push_in_any_order)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 1;
  auto test =
    test_type{ initial_push_size, producer_1_push_size, producer_2_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] = push_result::pushed;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(
    permutations,
    UnorderedElementsAre(vector<int>{ 100, 101 }, vector<int>{ 101, 100 }));
}

TEST_F(test_pushing_is_atomic_or_fails,
       three_successful_pushes_push_in_any_order)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 1;
  auto producer_3_push_size = 1;
  auto test = test_type{ initial_push_size,
                         producer_1_push_size,
                         producer_2_push_size,
                         producer_3_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] = push_result::pushed;
  test.producer_push_results[2] = push_result::pushed;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(permutations,
              UnorderedElementsAre(vector<int>{ 100, 101, 102 },
                                   vector<int>{ 100, 102, 101 },
                                   vector<int>{ 101, 100, 102 },
                                   vector<int>{ 101, 102, 100 },
                                   vector<int>{ 102, 100, 101 },
                                   vector<int>{ 102, 101, 100 }));
}

TEST_F(test_pushing_is_atomic_or_fails, two_successful_pushes_are_atomic)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 2;
  auto producer_2_push_size = 2;
  auto test =
    test_type{ initial_push_size, producer_1_push_size, producer_2_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] = push_result::pushed;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(
    permutations,
    UnorderedElementsAre(
      vector<int>{ 100, 101, 102, 103 }, // Producer 1, then producer 2.
      vector<int>{ 102, 103, 100, 101 }  // Producer 2, then producer 1.
      ));
}

TEST_F(test_pushing_is_atomic_or_fails,
       failed_pushes_leave_initial_push_unchanged)
{
  auto initial_push_size = 2;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 1;

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::not_pushed_due_to_contention;
    test.producer_push_results[1] = push_result::not_pushed_due_to_contention;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations, UnorderedElementsAre(vector<int>{ 100, 101 }));
  }

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::not_pushed_due_to_contention;
    test.producer_push_results[1] = push_result::pushed;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(vector<int>{ 100, 101, 103 }));
  }

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::pushed;
    test.producer_push_results[1] = push_result::not_pushed_due_to_contention;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(vector<int>{ 100, 101, 102 }));
  }
}

TEST_F(test_pushing_is_atomic_or_fails,
       interrupted_pushes_leave_initial_push_unchanged)
{
  auto initial_push_size = 2;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 1;

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] =
      push_result::push_interrupted_due_to_preemption;
    test.producer_push_results[1] =
      push_result::push_interrupted_due_to_preemption;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations, UnorderedElementsAre(vector<int>{ 100, 101 }));
  }

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] =
      push_result::push_interrupted_due_to_preemption;
    test.producer_push_results[1] = push_result::pushed;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(vector<int>{ 100, 101, 103 }));
  }

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::pushed;
    test.producer_push_results[1] =
      push_result::push_interrupted_due_to_preemption;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(vector<int>{ 100, 101, 102 }));
  }
}

TEST_F(test_pushing_is_atomic_or_fails,
       independently_overflowing_pushes_overflow)
{
  auto initial_push_size = 3;
  auto producer_1_push_size = 2;
  auto producer_2_push_size = 2;
  auto test =
    test_type{ initial_push_size, producer_1_push_size, producer_2_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] = push_result::pushed;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(permutations,
              UnorderedElementsAre(
                // Producer 1, then producer 2.
                vector<int>{ 103, 104, 105, 106 },
                // Producer 2, then producer 1.
                vector<int>{ 105, 106, 103, 104 }));
}

TEST_F(test_pushing_is_atomic_or_fails, mutually_overflowing_pushes_overflow)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 3;
  auto producer_2_push_size = 3;
  auto test =
    test_type{ initial_push_size, producer_1_push_size, producer_2_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] = push_result::pushed;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(permutations,
              UnorderedElementsAre(
                // Producer 1, then producer 2.
                vector<int>{ 102, 103, 104, 105 },
                // Producer 2, then producer 1.
                vector<int>{ 105, 100, 101, 102 }));
}

TEST_F(test_pushing_is_atomic_or_fails,
       failed_mutually_overflowing_pushes_preserve_pushed_items)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 3;
  auto producer_2_push_size = 3;

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::pushed;
    test.producer_push_results[1] = push_result::not_pushed_due_to_contention;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(vector<int>{ 100, 101, 102 }));
  }

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::not_pushed_due_to_contention;
    test.producer_push_results[1] = push_result::pushed;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(vector<int>{ 103, 104, 105 }));
  }
}

TEST_F(test_pushing_is_atomic_or_fails,
       interrupted_mutually_overflowing_pushes_may_overwrite_pushed_items)
{
  auto initial_push_size = 0;
  auto producer_1_push_size = 3;
  auto producer_2_push_size = 3;

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::pushed;
    test.producer_push_results[1] =
      push_result::push_interrupted_due_to_preemption;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(
                  // Either:
                  // * Producer 1 only. Producer 2's push was interrupted early
                  // (either before or after producer 1).
                  // * Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ 100, 101, 102 },
                  // Producer 1, then producer 2 (interrupted; overwrote).
                  vector<int>{ 102 }));
  }

  {
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] =
      push_result::push_interrupted_due_to_preemption;
    test.producer_push_results[1] = push_result::pushed;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(
                  // Either:
                  // * Producer 1 only. Producer 2's push was interrupted early
                  // (either before or after producer 1).
                  // * Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ 103, 104, 105 },
                  // Producer 1, then producer 2 (interrupted; overwrote).
                  vector<int>{ 105 }));
  }
}

TEST_F(test_pushing_is_atomic_or_fails,
       failed_overflowing_push_leaves_initial_push_unchanged)
{
  auto initial_push_size = 2;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 3;
  auto test =
    test_type{ initial_push_size, producer_1_push_size, producer_2_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] = push_result::not_pushed_due_to_contention;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(permutations, UnorderedElementsAre(vector<int>{ 100, 101, 102 }));
}

TEST_F(test_pushing_is_atomic_or_fails,
       interrupted_overflowing_push_possibly_overwrites_initial_push)
{
  {
    auto initial_push_size = 2;
    auto producer_1_push_size = 1;
    auto producer_2_push_size = 3;
    auto test = test_type{ initial_push_size,
                           producer_1_push_size,
                           producer_2_push_size };
    test.producer_push_results[0] = push_result::pushed;
    test.producer_push_results[1] =
      push_result::push_interrupted_due_to_preemption;
    auto permutations = test.expected_items_permutations(memory);
    EXPECT_THAT(permutations,
                UnorderedElementsAre(
                  // Producer 1 only. Producer 2's push was interrupted early
                  // (either before or after producer 1).
                  vector<int>{ 100, 101, 102 },
                  // Producer 1, then producer 2 (interrupted; overwrote).
                  vector<int>{ 102 },
                  // Producer 2 (interrupted; overwrote), then producer 1.
                  vector<int>{ 101, 102 }));
  }

  auto initial_push_size = 2;
  auto producer_1_push_size = 1;
  auto producer_2_push_size = 2;
  auto test =
    test_type{ initial_push_size, producer_1_push_size, producer_2_push_size };
  test.producer_push_results[0] = push_result::pushed;
  test.producer_push_results[1] =
    push_result::push_interrupted_due_to_preemption;
  auto permutations = test.expected_items_permutations(memory);
  EXPECT_THAT(permutations,
              UnorderedElementsAre(
                // Producer 1 only. Producer 2's push was interrupted early
                // (either before or after producer 1).
                vector<int>{ 100, 101, 102 },
                // Either:
                // * Producer 1, then producer 2 (interrupted; overwrote).
                // * Producer 2 (interrupted; overwrote), then producer 1.
                vector<int>{ 101, 102 }));
}

TEST(test_for_each_subset, empty_input)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(
    v{}, [&subsets](const v& subset) -> void { subsets.emplace_back(subset); });
  EXPECT_THAT(subsets, UnorderedElementsAre(v{}));
}

TEST(test_for_each_subset, single_item)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(v{ 42 }, [&subsets](const v& subset) -> void {
    subsets.emplace_back(subset);
  });
  EXPECT_THAT(subsets, UnorderedElementsAre(v{}, v{ 42 }));
}

TEST(test_for_each_subset, two_items)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(v{ 42, 9000 }, [&subsets](const v& subset) -> void {
    subsets.emplace_back(subset);
  });
  EXPECT_THAT(subsets,
              UnorderedElementsAre(v{}, v{ 42 }, v{ 9000 }, v{ 42, 9000 }));
}

TEST(test_for_each_subset, three_items)
{
  using v = std::vector<int>;
  auto subsets = std::vector<v>{};
  for_each_subset(v{ 42, 9000, -1 }, [&subsets](const v& subset) -> void {
    subsets.emplace_back(subset);
  });
  EXPECT_THAT(subsets,
              UnorderedElementsAre(v{},
                                   v{ 42 },
                                   v{ 9000 },
                                   v{ -1 },
                                   v{ 42, 9000 },
                                   v{ 42, -1 },
                                   v{ 9000, -1 },
                                   v{ 42, 9000, -1 }));
}
#endif

#if !CXXTRACE_ENABLE_GTEST
template<
  template<class T, std::size_t Capacity, class Index = int, class Sync = sync>
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
}

template<
  template<class T, std::size_t Capacity, class Index = int, class Sync = sync>
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

#if 0 // @@@ too slow for processor_local_mpsc_ring_queue
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
#endif

  // @@@ This test fails because it assumes the queue can hold at most size
  // items. It can actually hold size*processor_count items.
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
#if 0
  register_single_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::spsc_ring_queue>();
  register_single_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::mpsc_ring_queue>();
  register_multiple_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::mpsc_ring_queue>();
#endif
  register_multiple_producer_ring_queue_concurrency_tests<
    cxxtrace::detail::processor_local_mpsc_ring_queue>();
}
#endif
}

namespace {
#if !CXXTRACE_ENABLE_GTEST
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
#endif

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
