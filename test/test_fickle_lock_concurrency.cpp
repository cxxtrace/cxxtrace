#include "cxxtrace_concurrency_test.h"
#include <algorithm> // @nocommit std::count
#include <array>
#include <cassert>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/fickle_lock.h>
#include <numeric>

namespace cxxtrace_test {
class lock_can_be_acquired_by_one_thread
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);
    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    auto executed_count = 0;
    auto acquired = this->lock.try_with_lock(
      thread_id, [&] { executed_count += 1; }, CXXTRACE_HERE);
    CXXTRACE_ASSERT(acquired);
    CXXTRACE_ASSERT(executed_count == 1);

    auto acquired_again = this->lock.try_with_lock(
      thread_id, [&] { executed_count += 1; }, CXXTRACE_HERE);
    CXXTRACE_ASSERT(acquired_again);
    CXXTRACE_ASSERT(executed_count == 2);
  }

  auto tear_down() -> void {}

  cxxtrace::detail::fickle_lock lock;
};

class lock_allows_at_most_one_thread_concurrently
{
public:
  auto run_thread(int thread_index) -> void
  {
    assert(thread_index >= 0);
    assert(thread_index < static_cast<int>(this->acquired.size()));

    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    auto acquired =
      this->lock.try_with_lock(thread_id,
                               [&] {
                                 auto c = this->enter_count.load(CXXTRACE_HERE);
                                 this->enter_count.store(c + 1, CXXTRACE_HERE);
                               },
                               CXXTRACE_HERE);
    this->acquired[thread_index] = acquired;
  }

  auto tear_down() -> void
  {
    auto acquired_count =
      std::count(this->acquired.begin(), this->acquired.end(), true);
    CXXTRACE_ASSERT(acquired_count == enter_count.load(CXXTRACE_HERE));
  }

  static inline constexpr auto max_threads = 3;

  cxxtrace::detail::fickle_lock lock;
  cxxtrace::detail::nonatomic<int> enter_count{ 0 };
  std::array<bool, max_threads> acquired;
};

// @nocommit rename
class lock_allows_at_most_one_thread_concurrently_2
{
public:
  auto run_thread(int thread_index) -> void
  {
    assert(thread_index >= 0);
    assert(thread_index <
           static_cast<int>(this->thread_acquired_counts.size()));

    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    auto thread_acquired_count = 0;
    auto try_lock_count = thread_index == 0 ? 2 : 1;
    for (auto i = 0; i < try_lock_count; ++i) {
      auto acquired = this->lock.try_with_lock(
        thread_id,
        [&] {
          auto c = this->enter_count.load(CXXTRACE_HERE);
          this->enter_count.store(c + 1, CXXTRACE_HERE);
        },
        CXXTRACE_HERE);
      if (acquired) {
        thread_acquired_count += 1;
      }
    }
    this->thread_acquired_counts[thread_index] = thread_acquired_count;
  }

  auto tear_down() -> void
  {
    auto total_acquired_count = std::reduce(
      this->thread_acquired_counts.begin(), this->thread_acquired_counts.end());
    CXXTRACE_ASSERT(total_acquired_count == enter_count.load(CXXTRACE_HERE));
  }

  static inline constexpr auto max_threads = 3;

  cxxtrace::detail::fickle_lock lock;
  cxxtrace::detail::nonatomic<int> enter_count{ 0 };
  std::array<int, max_threads> thread_acquired_counts;
};

class try_lock_succeeds_iff_critical_section_was_executed
{
public:
  auto run_thread(int thread_index) -> void
  {
    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    for (auto i = 0; i < 2; ++i) {
      auto executed = false;
      auto acquired = this->lock.try_with_lock(
        thread_id, [&] { executed = true; }, CXXTRACE_HERE);
      CXXTRACE_ASSERT(acquired == executed);
    }
  }

  auto tear_down() -> void {}

  cxxtrace::detail::fickle_lock lock;
};

class try_lock_fails_if_leader_holds_lock
{
public:
  static inline constexpr auto leader_thread_index = int{ 0 };
  static inline constexpr auto leader_thread_id =
    cxxtrace::thread_id{ 0 }; // @nocommit make a index->id function

  explicit try_lock_fails_if_leader_holds_lock(int thread_count)
    : outsider_count{ thread_count - 1 }
  {
    assert(this->outsider_count > 0);
    this->set_leader();
  }

  auto set_leader() -> void
  {
    auto acquired =
      this->lock.try_with_lock(this->leader_thread_id, [] {}, CXXTRACE_HERE);
    assert(acquired);
  }

  auto run_thread(int thread_index) -> void
  {
    // @nocommit make a index->id function
    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    if (thread_index == this->leader_thread_index) {
      auto acquired = this->lock.try_with_lock(
        thread_id,
        [&] {
          this->leader_acquired_lock.store(true, CXXTRACE_HERE);
          auto backoff = cxxtrace_test::backoff{};
          while (this->exiting_outsiders.load(CXXTRACE_HERE) <
                 this->outsider_count) { // @nocommit semphore
            backoff.yield(CXXTRACE_HERE);
          }
        },
        CXXTRACE_HERE);
      CXXTRACE_ASSERT(acquired);
    } else {
      auto backoff = cxxtrace_test::backoff{};
      while (
        !this->leader_acquired_lock.load(CXXTRACE_HERE)) { // @nocommit semphore
        backoff.yield(CXXTRACE_HERE);
      }
      auto acquired = this->lock.try_with_lock(thread_id, [] {}, CXXTRACE_HERE);
      CXXTRACE_ASSERT(!acquired);
      this->exiting_outsiders.fetch_add(1, CXXTRACE_HERE);
    }
  }

  auto tear_down() -> void
  {
    assert(this->exiting_outsiders.load(CXXTRACE_HERE) == this->outsider_count);
  }

  int outsider_count;
  cxxtrace::detail::fickle_lock lock;
  cxxtrace::detail::atomic<bool> leader_acquired_lock{ false };
  cxxtrace::detail::atomic<int> exiting_outsiders{ 0 };
};

#if 0
// @nocommit rename
class try_lock_fails_if_leader_holds_lock_2
{
public:
  static inline constexpr auto original_leader_thread_index = int{0};
  static inline constexpr auto original_leader_thread_id = cxxtrace::thread_id{0}; // @nocommit make a index->id function
  static inline constexpr auto new_leader_thread_index = int{1};

  explicit try_lock_fails_if_leader_holds_lock_2() {
    assert(this->outsider_count > 0);
    this->set_leader();
  }

  auto set_leader() -> void {
    auto acquired = this->lock.try_with_lock(this->old_leader_thread_id, [] {
    }, CXXTRACE_HERE);
    assert(acquired);
  }

  auto run_thread(int thread_index) -> void
  {
    // @nocommit make a index->id function
    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    if (thread_index == this->old_leader_thread_index) {
    } else if (thread_index == this->new_leader_thread_index) {
      auto acquired = this->lock.try_with_lock(thread_id, [&] {
          this->leader_acquired_lock.store(true, CXXTRACE_HERE);
          auto backoff = cxxtrace_test::backoff{};
          while (this->exiting_outsiders.load(CXXTRACE_HERE) < this->outsider_count) { // @nocommit semphore
            backoff.yield(CXXTRACE_HERE);
          }
      }, CXXTRACE_HERE);
      CXXTRACE_ASSERT(acquired);
    } else {
      auto backoff = cxxtrace_test::backoff{};
      while (!this->leader_acquired_lock.load(CXXTRACE_HERE)) { // @nocommit semphore
        backoff.yield(CXXTRACE_HERE);
      }
      auto acquired = this->lock.try_with_lock(thread_id, [] {
      }, CXXTRACE_HERE);
      CXXTRACE_ASSERT(!acquired);
      this->exiting_outsiders.fetch_add(1, CXXTRACE_HERE);
    }
  }

  auto tear_down() -> void
  {
    assert(this->exiting_outsiders.load(CXXTRACE_HERE) == this->outsider_count);
  }

  int outsider_count;
  cxxtrace::detail::fickle_lock lock;
  cxxtrace::detail::atomic<bool> leader_acquired_lock{false};
  cxxtrace::detail::atomic<int> exiting_outsiders{0};
};
#endif

// @nocommit this test is identical to try_lock_fails_if_leader_holds_lock, but
// with one extra assertion. factor?
class
  failing_try_lock_when_leader_holds_lock_does_not_prevent_leader_from_relocking
{
public:
  static inline constexpr auto leader_thread_index = int{ 0 };
  static inline constexpr auto leader_thread_id =
    cxxtrace::thread_id{ 0 }; // @nocommit make a index->id function

  explicit failing_try_lock_when_leader_holds_lock_does_not_prevent_leader_from_relocking(
    int thread_count)
    : outsider_count{ thread_count - 1 }
  {
    assert(this->outsider_count > 0);
    this->set_leader();
  }

  auto set_leader() -> void
  {
    auto acquired =
      this->lock.try_with_lock(this->leader_thread_id, [] {}, CXXTRACE_HERE);
    assert(acquired);
  }

  auto run_thread(int thread_index) -> void
  {
    // @nocommit make a index->id function
    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    if (thread_index == this->leader_thread_index) {
      auto acquired = this->lock.try_with_lock(
        thread_id,
        [&] {
          this->leader_acquired_lock.store(true, CXXTRACE_HERE);
          // @nocommit ideally this test should only wait until
          // lock_held_by_leader is loaded by try_take_leadership_and_lock.
          auto backoff = cxxtrace_test::backoff{};
          while (this->exiting_outsiders.load(CXXTRACE_HERE) <
                 this->outsider_count) { // @nocommit semphore
            backoff.yield(CXXTRACE_HERE);
          }
        },
        CXXTRACE_HERE);
      CXXTRACE_ASSERT(acquired);

      auto acquired_again =
        this->lock.try_with_lock(thread_id, [] {}, CXXTRACE_HERE);
      CXXTRACE_ASSERT(acquired_again);
    } else {
      auto backoff = cxxtrace_test::backoff{};
      while (
        !this->leader_acquired_lock.load(CXXTRACE_HERE)) { // @nocommit semphore
        backoff.yield(CXXTRACE_HERE);
      }
      auto acquired = this->lock.try_with_lock(thread_id, [] {}, CXXTRACE_HERE);
      CXXTRACE_ASSERT(!acquired);
      this->exiting_outsiders.fetch_add(1, CXXTRACE_HERE);
    }
  }

  auto tear_down() -> void
  {
    assert(this->exiting_outsiders.load(CXXTRACE_HERE) == this->outsider_count);
  }

  int outsider_count;
  cxxtrace::detail::fickle_lock lock;
  cxxtrace::detail::atomic<bool> leader_acquired_lock{ false };
  cxxtrace::detail::atomic<int> exiting_outsiders{ 0 };
};

class try_lock_succeeds_for_uncontended_lock
{
public:
  auto run_thread(int thread_index) -> void
  {
    auto thread_id = static_cast<cxxtrace::thread_id>(thread_index);
    assert(thread_id != cxxtrace::invalid_thread_id);

    auto backoff = cxxtrace_test::backoff{};
    while (this->desired_leader_thread_index.load(CXXTRACE_HERE) !=
           thread_index) { // @nocommit semaphore
      backoff.yield(CXXTRACE_HERE);
    }

    auto acquired = this->lock.try_with_lock(thread_id, [] {}, CXXTRACE_HERE);
    CXXTRACE_ASSERT(acquired);
    this->desired_leader_thread_index.store(thread_index + 1, CXXTRACE_HERE);
  }

  auto tear_down() -> void {}

  cxxtrace::detail::fickle_lock lock;
  cxxtrace::detail::atomic<int> desired_leader_thread_index{ 0 };
};

// @nocommit test: on mass contention, at least one thread succeeds?
// @nocommit test: on mass contention (including leader), at least one thread
// succeeds?

auto
register_concurrency_tests() -> void
{
#if SLOW
  register_concurrency_test<lock_allows_at_most_one_thread_concurrently_2>(
    3, concurrency_test_depth::full);
#endif

  // register_concurrency_test<try_lock_fails_if_leader_holds_lock_2>(3,
  // concurrency_test_depth::full, 3);
  register_concurrency_test<lock_can_be_acquired_by_one_thread>(
    1, concurrency_test_depth::full);
  register_concurrency_test<lock_allows_at_most_one_thread_concurrently>(
    2, concurrency_test_depth::full);
  register_concurrency_test<lock_allows_at_most_one_thread_concurrently>(
    3, concurrency_test_depth::shallow);
  register_concurrency_test<
    try_lock_succeeds_iff_critical_section_was_executed>(
    2, concurrency_test_depth::shallow);
#if SLOW // @nocommit
  register_concurrency_test<
    try_lock_succeeds_iff_critical_section_was_executed>(
    3, concurrency_test_depth::shallow);
#endif
  register_concurrency_test<try_lock_fails_if_leader_holds_lock>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<try_lock_fails_if_leader_holds_lock>(
    3, concurrency_test_depth::shallow, 3);
  register_concurrency_test<
    failing_try_lock_when_leader_holds_lock_does_not_prevent_leader_from_relocking>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<try_lock_succeeds_for_uncontended_lock>(
    2, concurrency_test_depth::full);
  register_concurrency_test<try_lock_succeeds_for_uncontended_lock>(
    3, concurrency_test_depth::shallow); // @nocommit easily s/shallow/full/
                                         // with a semaphore
}
}
