#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "rseq_scheduler.h"
#include "synchronization.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/processor.h>
#include <ostream>
#include <string>
// IWYU pragma: no_include "relacy_backoff.h"
// IWYU pragma: no_include "relacy_synchronization.h"
// IWYU pragma: no_include <cxxtrace/detail/real_synchronization.h>
// IWYU pragma: no_include <relacy/context_base_impl.hpp>

#if CXXTRACE_ENABLE_RELACY
#include <relacy/context_base.hpp>
#endif

namespace cxxtrace_test {
using sync = concurrency_test_synchronization;

// Test per-processor counters implemented using rseq.
//
// Specifically, test that incrementing the counter is atomic.
//
// This counter implementation is interesting because it does not involve any
// RMW operations.
//
// NOTE(strager): This test is a manual test. See the documentation for bug_1,
// bug_2, and bug_3 below.
// TODO(strager): Extend the test framework to make it possible to assert test
// failures, automating this test.
class test_counter : public rseq_scheduler_test_base
{
public:
  // If at least one of bug_1, bug_2, and bug_3 is true, this test should fail
  // with an assertion error.
  static constexpr auto bug_1 = false;
  static constexpr auto bug_2 = false;
  static constexpr auto bug_3 = false;

  explicit test_counter(int rseq_attempt_count)
    : rseq_attempt_count{ rseq_attempt_count }
  {
    assert(rseq_attempt_count >= 1);
    std::fill(this->thread_did_update_counter.begin(),
              this->thread_did_update_counter.end(),
              false);
  }

  auto run_thread(int thread_index) -> void
  {
    assert(thread_index < int(this->thread_did_update_counter.size()));
    this->thread_did_update_counter[thread_index] = false;

    auto backoff = cxxtrace_test::backoff{};
    for (auto i = 0; i < this->rseq_attempt_count; ++i) {
      auto updated_counter = this->try_increment_counter();
      if (updated_counter) {
        this->thread_did_update_counter[thread_index] = true;
        break;
      }
      backoff.yield(CXXTRACE_HERE);
    }
  }

  auto try_increment_counter() -> bool
  {
    auto processor_id = cxxtrace::detail::processor_id{};
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      assert(processor_id < this->per_processor_counters.size());
      this->rseq.allow_preempt(CXXTRACE_HERE);

      auto& counter = this->per_processor_counters[processor_id].counter;
      auto counter_value = counter.load(this->bug_1 ? std::memory_order_acquire
                                                    : std::memory_order_seq_cst,
                                        CXXTRACE_HERE);
      this->rseq.allow_preempt(CXXTRACE_HERE);
      counter.store(counter_value + 1,
                    this->bug_2 ? std::memory_order_release
                                : std::memory_order_seq_cst,
                    CXXTRACE_HERE);

      // NOTE(strager): We must not call allow_preempt after storing to the
      // counter. Otherwise, we will have updated the counter but we would
      // forget that we updated it.
      if (this->bug_3) {
        this->rseq.allow_preempt(CXXTRACE_HERE);
      }
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return true;
  preempted:
    return false;
  }

  auto tear_down() -> void
  {
    auto expected_counter_total = 0;
    for (auto counter_updated : this->thread_did_update_counter) {
      expected_counter_total += counter_updated ? 1 : 0;
    }

    auto actual_counter_total = 0;
    for (auto& counter : this->per_processor_counters) {
      actual_counter_total += counter.counter.load(CXXTRACE_HERE);
    }

    concurrency_log(
      [&](std::ostream& out) {
        out << "expected_counter_total = " << expected_counter_total
            << "; actual_counter_total = " << actual_counter_total;
      },
      CXXTRACE_HERE);
    CXXTRACE_ASSERT(actual_counter_total == expected_counter_total);
  }

  struct processor_counter
  {
    sync::atomic<int> counter{ 0 };
  };

  static constexpr auto max_thread_count = 3;
  static constexpr auto max_processor_count = 3;

  int rseq_attempt_count;
  std::array<processor_counter, max_processor_count> per_processor_counters;
  std::array<bool, max_thread_count> thread_did_update_counter;
};

enum class spin_lock_style
{
  // Implement a basic, unoptimized spin lock.
  basic,

  // Simulate librseq's test spin lock as implemented for ARM:
  // https://github.com/compudj/librseq/blob/152600188dd214a0b2c6a8c66380e50c6ad27154/tests/basic_percpu_ops_test.c
  // https://github.com/compudj/librseq/blob/152600188dd214a0b2c6a8c66380e50c6ad27154/include/rseq/rseq-arm.h
  librseq_arm,

  // Simulate librseq's test spin lock as implemented for x86_64:
  // https://github.com/compudj/librseq/blob/152600188dd214a0b2c6a8c66380e50c6ad27154/tests/basic_percpu_ops_test.c
  // https://github.com/compudj/librseq/blob/152600188dd214a0b2c6a8c66380e50c6ad27154/include/rseq/rseq-x86.h
  librseq_x86_64,
};

// Test per-processor spin locks where the lock procedure is implemented using
// rseq.
//
// Specifically, test that the spin lock provides mutual exclusion.
//
// This spin lock implementation is interesting because it does not involve any
// RMW operations.
//
// NOTE(strager): This test is a manual test. See the documentation for bug_1
// and bug_2 below.
// TODO(strager): Extend the test framework to make it possible to assert test
// failures, automating this test.
template<spin_lock_style Style>
class test_spin_lock : public rseq_scheduler_test_base
{
public:
  // For spin_lock_style::basic, if bug_1 is true, this test should fail with
  // a data race error.
  static constexpr auto bug_1 = false;
  // For any style, if bug_2 is true, this test should fail with an assertion
  // error.
  static constexpr auto bug_2 = false;

  struct processor_data;

  explicit test_spin_lock(int rseq_attempt_count)
    : rseq_attempt_count{ rseq_attempt_count }
  {
    assert(rseq_attempt_count >= 1);
    std::fill(this->thread_did_update_counter.begin(),
              this->thread_did_update_counter.end(),
              false);
  }

  auto run_thread(int thread_index) -> void
  {
    assert(thread_index < int(this->thread_did_update_counter.size()));
    this->thread_did_update_counter[thread_index] = false;

    auto backoff = cxxtrace_test::backoff{};
    for (auto i = 0; i < this->rseq_attempt_count; ++i) {
      auto* processor_data = this->try_lock(backoff);
      if (processor_data) {
        this->critical_section(processor_data);
        processor_data->unlock();
        this->thread_did_update_counter[thread_index] = true;
        break;
      }
      backoff.yield(CXXTRACE_HERE);
    }
  }

  auto try_lock(cxxtrace_test::backoff& backoff) -> processor_data*
  {
    processor_data* data;
    auto acquired_lock = false;
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      assert(processor_id < this->per_processor_data.size());
      this->rseq.allow_preempt(CXXTRACE_HERE);

      assert(processor_id < this->per_processor_data.size());
      data = &this->per_processor_data[processor_id];
      for (auto i = 0; i < this->max_spin_count; ++i) {
        if (data->try_lock()) {
          acquired_lock = true;
          break;
        }
        this->rseq.allow_preempt(CXXTRACE_HERE);
        backoff.yield(CXXTRACE_HERE);
      }
      if (acquired_lock) {
        // NOTE(strager): We must not call allow_preempt after try_lock.
        // Otherwise, we will have acquired the lock but we would forget that we
        // acquired the lock.
        if (this->bug_2) {
          this->rseq.allow_preempt(CXXTRACE_HERE);
        }
      }
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    if (acquired_lock) {
      switch (Style) {
        case spin_lock_style::librseq_arm:
          // ARM assembly: dmb ish
          sync::atomic_thread_fence(std::memory_order_seq_cst, CXXTRACE_HERE);
          break;
        case spin_lock_style::librseq_x86_64:
          // x86_64 assembly: (compiler barrier)
          break;
        case spin_lock_style::basic:
          // No extra synchronization is necessary.
          break;
      }
      return data;
    } else {
      return nullptr;
    }
  preempted:
    return nullptr;
  }

  auto critical_section(processor_data* data) -> void
  {
    auto counter_value = data->counter.load(CXXTRACE_HERE);
    data->counter.store(counter_value + 1, CXXTRACE_HERE);
  }

  auto tear_down() -> void
  {
    this->assert_locks_are_not_held();
    this->assert_counter_was_updated();
  }

  auto assert_locks_are_not_held() -> void
  {
    for (auto& data : this->per_processor_data) {
      CXXTRACE_ASSERT(!data.is_lock_held.load(CXXTRACE_HERE));
    }
  }

  auto assert_counter_was_updated() -> void
  {
    auto expected_counter_total = 0;
    for (auto counter_updated : this->thread_did_update_counter) {
      expected_counter_total += counter_updated ? 1 : 0;
    }

    auto actual_counter_total = 0;
    for (auto& data : this->per_processor_data) {
      actual_counter_total += data.counter.load(CXXTRACE_HERE);
    }

    concurrency_log(
      [&](std::ostream& out) {
        out << "expected_counter_total = " << expected_counter_total
            << "; actual_counter_total = " << actual_counter_total;
      },
      CXXTRACE_HERE);
    CXXTRACE_ASSERT(actual_counter_total == expected_counter_total);
  }

  struct processor_data
  {
    auto try_lock() -> bool
    {
      bool was_lock_held;
      switch (Style) {
        case spin_lock_style::librseq_arm:
          // ARM assembly: ldr r0, [is_lock_held]
          was_lock_held =
            this->is_lock_held.load(std::memory_order_relaxed, CXXTRACE_HERE);
          break;
        case spin_lock_style::librseq_x86_64:
          // x86_64 assembly: cmpl (is_lock_held), %eax
          was_lock_held =
            this->is_lock_held.load(std::memory_order_acquire, CXXTRACE_HERE);
          break;
        case spin_lock_style::basic:
          was_lock_held =
            this->is_lock_held.load(std::memory_order_seq_cst, CXXTRACE_HERE);
          break;
      }
      rseq_scheduler<sync>::allow_preempt(CXXTRACE_HERE);
      if (was_lock_held) {
        return false;
      } else {
        switch (Style) {
          case spin_lock_style::librseq_arm:
            // ARM assembly: str [is_lock_held], r0
            this->is_lock_held.store(
              true, std::memory_order_relaxed, CXXTRACE_HERE);
            break;
          case spin_lock_style::librseq_x86_64:
            // x86_64 assembly: movl (is_lock_held), %eax
            this->is_lock_held.store(
              true, std::memory_order_release, CXXTRACE_HERE);
            break;
          case spin_lock_style::basic:
            this->is_lock_held.store(
              true, std::memory_order_seq_cst, CXXTRACE_HERE);
            break;
        }
        // We have acquired the lock.
        // NOTE(strager): We must not call allow_preempt after acquiring the
        // lock. Otherwise, we will have acquired the lock but we would forget
        // that we acquired the lock.
        return true;
      }
    }

    auto unlock() -> void
    {
      switch (Style) {
        case spin_lock_style::librseq_arm:
          // ARM assembly: dmb; str [is_lock_held], r0
          this->is_lock_held.store(
            false, std::memory_order_release, CXXTRACE_HERE);
          break;
        case spin_lock_style::librseq_x86_64:
          // x86_64 assembly: (compiler barrier); movl %eax, (is_lock_held)
          this->is_lock_held.store(
            false, std::memory_order_release, CXXTRACE_HERE);
          break;
        case spin_lock_style::basic:
          this->is_lock_held.store(false,
                                   bug_1 ? std::memory_order_relaxed
                                         : std::memory_order_release,
                                   CXXTRACE_HERE);
          break;
      }
    }

    sync::atomic<bool> is_lock_held{ 0 };
    sync::nonatomic<int> counter{ 0 };
  };

  static constexpr auto max_thread_count = 3;
  static constexpr auto max_processor_count = 3;

  int rseq_attempt_count;
  int max_spin_count{ 2 };
  std::array<processor_data, max_processor_count> per_processor_data;
  std::array<bool, max_thread_count> thread_did_update_counter;
};

auto
register_concurrency_tests() -> void
{
  register_concurrency_test<test_counter>(2, concurrency_test_depth::full, 3);
  register_concurrency_test<test_counter>(3, concurrency_test_depth::full, 1);

  register_concurrency_test<test_spin_lock<spin_lock_style::basic>>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<test_spin_lock<spin_lock_style::basic>>(
    3, concurrency_test_depth::full, 1);
  register_concurrency_test<test_spin_lock<spin_lock_style::librseq_arm>>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<test_spin_lock<spin_lock_style::librseq_x86_64>>(
    2, concurrency_test_depth::full, 2);
}
}
