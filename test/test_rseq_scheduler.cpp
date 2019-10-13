#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "rseq_scheduler.h"
#include "synchronization.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/processor.h>
#include <functional>
#include <numeric>
#include <ostream>
#include <string>
// IWYU pragma: no_include "relacy_synchronization.h"
// IWYU pragma: no_include <cxxtrace/detail/real_synchronization.h>
// IWYU pragma: no_include <relacy/context_base_impl.hpp>

#if CXXTRACE_ENABLE_RELACY
#include <relacy/base.hpp>
#include <relacy/context_base.hpp>
#endif

namespace cxxtrace_test {
using sync = concurrency_test_synchronization;

// NOTE(strager): This test is a manual test. Ensure the test prints 'result:
// preempted' and 'result: not preempted' at least one time each.
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class test_allow_preempt_call_is_preempted_sometimes
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);

    this->before_allow_preempt = true;
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->after_allow_preempt = true;
    return;

  preempted:
    this->preempted = true;
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->before_allow_preempt);
    CXXTRACE_ASSERT(this->after_allow_preempt || this->preempted);

    if (this->after_allow_preempt) {
      std::fprintf(stderr, "result: not preempted\n");
    }
    if (this->preempted) {
      std::fprintf(stderr, "result: preempted\n");
    }
  }

  bool before_allow_preempt{ false };
  bool after_allow_preempt{ false };
  bool preempted{ false };
};

// NOTE(strager): This test is a manual test. Ensure the test prints all of the
// following at least one time each:
// * result: preempted twice
// * result: preempted first critical section only
// * result: preempted second critical section only
// * result: never preempted
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class
  test_allow_preempt_call_is_preempted_sometimes_in_multiple_critical_sections
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    this->critical_section(0);
    this->critical_section(1);
  }

  auto critical_section(int index) -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->before_allow_preempt[index] = true;
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->after_allow_preempt[index] = true;
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;
  preempted:
    this->preempted[index] = true;
  }

  auto tear_down() -> void
  {
    auto check_critical_section = [this](int index) {
      CXXTRACE_ASSERT(this->before_allow_preempt[index]);
      CXXTRACE_ASSERT(this->after_allow_preempt[index] ||
                      this->preempted[index]);
    };
    check_critical_section(0);
    check_critical_section(1);

    if (this->preempted[0]) {
      if (this->preempted[1]) {
        std::fprintf(stderr, "result: preempted twice\n");
      } else {
        std::fprintf(stderr, "result: preempted first critical section only\n");
      }
    } else {
      if (this->preempted[1]) {
        std::fprintf(stderr,
                     "result: preempted second critical section only\n");
      } else {
        std::fprintf(stderr, "result: never preempted\n");
      }
    }
  }

  std::array<bool, 2> before_allow_preempt{ false, false };
  std::array<bool, 2> after_allow_preempt{ false, false };
  std::array<bool, 2> preempted{ false, false };
};

class test_allow_preempt_call_never_preempts_after_critical_section
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->rseq.end_preemptable(CXXTRACE_HERE);

    this->before_allow_preempt = true;
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->after_allow_preempt = true;
    return;

  preempted:
    this->preempted = true;
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->before_allow_preempt);
    CXXTRACE_ASSERT(this->after_allow_preempt);
    CXXTRACE_ASSERT(!this->preempted);
  }

  bool before_allow_preempt{ false };
  bool after_allow_preempt{ false };
  bool preempted{ false };
};

class test_allow_preempt_call_never_preempts_after_aborting_critical_section
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:
    this->preempted_count += 1;
    this->rseq.allow_preempt(CXXTRACE_HERE);
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->preempted_count == 0 || this->preempted_count == 1);
  }

  int preempted_count{ 0 };
};

class test_allow_preempt_call_never_preempts_before_critical_section
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    this->before_allow_preempt = true;
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->after_allow_preempt = true;
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->before_allow_preempt);
    CXXTRACE_ASSERT(this->after_allow_preempt);
  }

  bool before_allow_preempt{ false };
  bool after_allow_preempt{ false };
};

class test_preemptable_state_resets_between_iterations
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_ASSERT(!this->rseq.in_critical_section());

    // Coerce model checkers into executing another iteration.
    concurrency_rng_next_integer_0(2);

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    // Avoid calling end_preemptable so we exit this iteration with
    // in_critical_section()==true.
    CXXTRACE_ASSERT(this->rseq.in_critical_section());
    return;

  preempted:
    CXXTRACE_ASSERT(false);
  }

  auto tear_down() -> void {}
};

class test_allow_preempt_call_never_preempts_between_critical_sections
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    this->critical_section(0);
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->critical_section(1);
  }

  auto critical_section(int index) -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->not_preempted_count[index] += 1;
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:
    this->preempted_count[index] += 1;
  }

  auto tear_down() -> void
  {
    auto check_critical_section = [this](int index) {
      CXXTRACE_ASSERT(
        this->preempted_count[index] + this->not_preempted_count[index] == 1);
      CXXTRACE_ASSERT(this->preempted_count[index] == 0 ||
                      this->preempted_count[index] == 1);
      CXXTRACE_ASSERT(this->not_preempted_count[index] == 0 ||
                      this->not_preempted_count[index] == 1);
    };
    check_critical_section(0);
    check_critical_section(1);
  }

  std::array<int, 2> not_preempted_count{ 0, 0 };
  std::array<int, 2> preempted_count{ 0, 0 };
};

// NOTE(strager): This test is a manual test. Ensure the test prints all of the
// following at least one time each:
// * result: only thread 0 preempted
// * result: only thread 1 preempted
// * result: both threads preempted
// * result: neither thread preempted
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class test_allow_preempt_calls_on_different_threads_are_preempted_independently
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    assert(thread_index == 0 || thread_index == 1);
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->before_allow_preempt[thread_index] = true;
    this->rseq.allow_preempt(CXXTRACE_HERE);
    this->after_allow_preempt[thread_index] = true;
    return;

  preempted:
    this->preempted[thread_index] = true;
  }

  auto tear_down() -> void
  {
    auto check_critical_section = [this](int index) {
      CXXTRACE_ASSERT(this->before_allow_preempt[index]);
      CXXTRACE_ASSERT(this->after_allow_preempt[index] ||
                      this->preempted[index]);
    };
    check_critical_section(0);
    check_critical_section(1);

    if (this->preempted[0]) {
      if (this->preempted[1]) {
        std::fprintf(stderr, "result: both threads preempted\n");
      } else {
        std::fprintf(stderr, "result: only thread 0 preempted\n");
      }
    } else {
      if (this->preempted[1]) {
        std::fprintf(stderr, "result: only thread 1 preempted\n");
      } else {
        std::fprintf(stderr, "result: neither thread preempted\n");
      }
    }
  }

  std::array<bool, 2> before_allow_preempt{ false, false };
  std::array<bool, 2> after_allow_preempt{ false, false };
  std::array<bool, 2> preempted{ false, false };
};

class test_current_processor_id_is_constant_within_critical_section
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread([[maybe_unused]] int thread_index) -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      for (auto i = 0; i < 4; ++i) {
        this->rseq.allow_preempt(CXXTRACE_HERE);
        CXXTRACE_ASSERT(this->rseq.get_current_processor_id(CXXTRACE_HERE) ==
                        processor_id);
      }
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
  }

  auto tear_down() -> void {}
};

class test_threads_have_different_processor_ids_within_critical_section
  : public rseq_scheduler_test_base
{
public:
  explicit test_threads_have_different_processor_ids_within_critical_section(
    int processor_count) noexcept
    : rseq_scheduler_test_base{ processor_count }
  {
    for (auto& thread_processor_id : this->thread_processor_ids) {
      thread_processor_id.store(this->invalid_processor_id, CXXTRACE_HERE);
    }
  }

  auto run_thread(int thread_index) -> void
  {
    auto reset_processor_id_for_current_thread = [&] {
      this->thread_processor_ids[thread_index].store(
        this->invalid_processor_id, std::memory_order_seq_cst, CXXTRACE_HERE);
    };

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      this->rseq.set_preempt_callback([&reset_processor_id_for_current_thread] {
        reset_processor_id_for_current_thread();
      });

      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);

      auto assert_other_threads_are_not_executing_on_current_processor = [&] {
        for (auto other_thread_index = 0;
             other_thread_index < int(this->thread_processor_ids.size());
             ++other_thread_index) {
          if (other_thread_index == thread_index) {
            continue;
          }
          auto other_thread_processor_id =
            this->thread_processor_ids[other_thread_index].load(
              std::memory_order_seq_cst, CXXTRACE_HERE);
          if (other_thread_processor_id != this->invalid_processor_id) {
            CXXTRACE_ASSERT(other_thread_processor_id != processor_id);
          }
        }
      };

      this->thread_processor_ids[thread_index].store(
        processor_id, std::memory_order_seq_cst, CXXTRACE_HERE);
      this->rseq.allow_preempt(CXXTRACE_HERE);
      assert_other_threads_are_not_executing_on_current_processor();
      reset_processor_id_for_current_thread();
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
    // NOTE(strager): reset_processor_id_for_current_thread() has already been
    // called.
  }

  auto tear_down() -> void {}

  static constexpr auto invalid_processor_id =
    ~cxxtrace::detail::processor_id{ 0 };
  static constexpr auto thread_count = 2;
  std::array<sync::atomic<cxxtrace::detail::processor_id>, thread_count>
    thread_processor_ids;
};

class
  test_threads_outside_critical_sections_have_different_processor_ids_than_threads_inside_critical_sections
  : public rseq_scheduler_test_base
{
public:
  explicit test_threads_outside_critical_sections_have_different_processor_ids_than_threads_inside_critical_sections(
    int processor_count) noexcept
    : rseq_scheduler_test_base{ processor_count }
  {
    for (auto& thread_processor_id : this->thread_processor_ids) {
      thread_processor_id.store(this->invalid_processor_id, CXXTRACE_HERE);
    }
  }

  auto run_thread(int thread_index) -> void
  {
    if (this->should_thread_enter_critical_section(thread_index)) {
      this->run_thread_inside_critical_section(thread_index);
    } else {
      this->run_thread_outside_critical_section();
    }
  }

  auto should_thread_enter_critical_section(int thread_index) noexcept -> bool
  {
    return thread_index == 0;
  }

  auto run_thread_outside_critical_section() -> void
  {
    auto
      assert_other_thread_in_critical_section_is_running_on_a_different_processor =
        [this](int other_thread_index) -> void {
      auto other_thread_processor_id =
        this->thread_processor_ids[other_thread_index].load(
          std::memory_order_seq_cst, CXXTRACE_HERE);
      if (other_thread_processor_id == this->invalid_processor_id) {
        // The other thread has not yet entered the critical section, or the
        // other thread has already exited the critical section.
        return;
      }

      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);

      auto other_thread_processor_id_again =
        this->thread_processor_ids[other_thread_index].load(
          std::memory_order_seq_cst, CXXTRACE_HERE);
      if (other_thread_processor_id_again == this->invalid_processor_id) {
        // The other thread exited the critical section. We don't know whether
        // it exited before or after our call to get_current_processor_id. In
        // other words, we don't know if this thread was scheduled on the other
        // thread's processor. Skip this execution.
        return;
      }
      CXXTRACE_ASSERT(other_thread_processor_id_again ==
                      other_thread_processor_id);

      CXXTRACE_ASSERT(processor_id != other_thread_processor_id);
    };

    for (auto other_thread_index = 0;
         other_thread_index < int(this->thread_processor_ids.size());
         ++other_thread_index) {
      if (!this->should_thread_enter_critical_section(other_thread_index)) {
        continue;
      }
      assert_other_thread_in_critical_section_is_running_on_a_different_processor(
        other_thread_index);
    }
  }

  auto run_thread_inside_critical_section(int thread_index) -> void
  {
    auto reset_processor_id_for_current_thread = [&] {
      this->thread_processor_ids[thread_index].store(
        this->invalid_processor_id, std::memory_order_seq_cst, CXXTRACE_HERE);
    };

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      this->rseq.set_preempt_callback([&reset_processor_id_for_current_thread] {
        reset_processor_id_for_current_thread();
      });

      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);

      this->thread_processor_ids[thread_index].store(
        processor_id, std::memory_order_seq_cst, CXXTRACE_HERE);
      this->rseq.allow_preempt(CXXTRACE_HERE);
      reset_processor_id_for_current_thread();
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
    // NOTE(strager): reset_processor_id_for_current_thread() has already been
    // called.
  }

  auto tear_down() -> void {}

  static constexpr auto invalid_processor_id =
    ~cxxtrace::detail::processor_id{ 0 };
  static constexpr auto thread_count = 3;
  std::array<sync::atomic<cxxtrace::detail::processor_id>, thread_count>
    thread_processor_ids;
};

// NOTE(strager): This test is a manual test. Ensure the test prints all of the
// following at least one time each:
// * result: thread 1 preempted thread 0
// * result: thread 0 preempted thread 1
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class
  test_preempted_thread_might_share_processor_with_another_thread_in_critical_section
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    auto reset_processor_id_for_current_thread = [&] {
      this->thread_processor_ids[thread_index].store(
        this->invalid_processor_id, std::memory_order_seq_cst, CXXTRACE_HERE);
    };

    auto other_thread_index = 1 - thread_index;

    volatile auto processor_id = this->invalid_processor_id;
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);

    this->rseq.set_preempt_callback([&reset_processor_id_for_current_thread] {
      reset_processor_id_for_current_thread();
    });

    processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
    CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);

    this->thread_processor_ids[thread_index].store(
      processor_id, std::memory_order_seq_cst, CXXTRACE_HERE);

    this->rseq.allow_preempt(CXXTRACE_HERE);
    reset_processor_id_for_current_thread();
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:
    // NOTE(strager): reset_processor_id_for_current_thread() has already been
    // called.

    CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);
    auto other_thread_processor_id =
      this->thread_processor_ids[other_thread_index].load(
        std::memory_order_seq_cst, CXXTRACE_HERE);
    if (other_thread_processor_id == processor_id) {
      std::fprintf(stderr,
                   "result: thread %d preempted thread %d\n",
                   other_thread_index,
                   thread_index);
    }
  }

  auto tear_down() -> void {}

  static constexpr auto invalid_processor_id =
    ~cxxtrace::detail::processor_id{ 0 };
  std::array<sync::atomic<cxxtrace::detail::processor_id>, 2>
    thread_processor_ids{ invalid_processor_id, invalid_processor_id };
};

// NOTE(strager): This test is a manual test. Ensure the test prints all of the
// following at least one time each:
// * result: thread stayed on same processor
// * result: thread switched to another processor
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class test_thread_might_change_processor_when_entering_critical_section
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread([[maybe_unused]] int thread_index) -> void
  {
    auto processor_id_outside =
      this->rseq.get_current_processor_id(CXXTRACE_HERE);
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      auto processor_id_inside =
        this->rseq.get_current_processor_id(CXXTRACE_HERE);
      if (processor_id_inside == processor_id_outside) {
        std::fprintf(stderr, "result: thread stayed on same processor\n");
      } else {
        std::fprintf(stderr, "result: thread switched to another processor\n");
      }
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
  }

  auto tear_down() -> void {}
};

class test_thread_sees_stores_by_previous_thread_on_processor
  : public rseq_scheduler_test_base
{
public:
  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      assert(processor_id < this->processor_states.size());
      auto& state = this->processor_states[processor_id];

      this->rseq.allow_preempt(CXXTRACE_HERE);

      state.counter.store(
        state.counter.load(std::memory_order_relaxed, CXXTRACE_HERE) + 1,
        std::memory_order_relaxed,
        CXXTRACE_HERE);
      state.thread_updated_counter[thread_index] = true;
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
  }

  auto tear_down() -> void
  {
    for (auto& state : this->processor_states) {
      auto expected_counter =
        std::accumulate(state.thread_updated_counter.begin(),
                        state.thread_updated_counter.end(),
                        int{ 0 });
      auto actual_counter = state.counter.load(CXXTRACE_HERE);
      concurrency_log(
        [&](std::ostream& out) {
          out << "processor " << (&state - this->processor_states.data())
              << ": expected_counter = " << expected_counter
              << "; actual_counter = " << actual_counter;
        },
        CXXTRACE_HERE);
      CXXTRACE_ASSERT(actual_counter == expected_counter);
    }
  }

  static constexpr auto max_processor_count = 3;
  static constexpr auto max_thread_count = 3;

  struct processor_state
  {
    std::array<bool, max_thread_count> thread_updated_counter{};
    sync::atomic<int> counter{ 0 };
  };

  std::array<processor_state, max_processor_count> processor_states;
};

// NOTE(strager): This test is a manual test. Ensure the test prints the
// following at least one time:
// * result: consumer had no acquire fence
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class test_critical_section_does_not_force_acquire_fence
  : public rseq_scheduler_test_base
{
public:
  static constexpr auto force_acquire_fence = false;

  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    switch (thread_index) {
      case 0:
        this->run_consumer_thread();
        break;
      case 1:
        this->run_producer_thread();
        break;
      default:
        CXXTRACE_ASSERT(false);
        break;
    }
  }

  auto run_consumer_thread() -> void
  {
    cxxtrace::detail::processor_id first_processor_id;
    cxxtrace::detail::processor_id second_processor_id;
    bool is_data_stored;
    int data_after;

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    first_processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
    is_data_stored =
      this->is_data_stored.load(std::memory_order_relaxed, CXXTRACE_HERE);
    this->rseq.end_preemptable(CXXTRACE_HERE);

    if (force_acquire_fence) {
      sync::atomic_thread_fence(std::memory_order_acquire, CXXTRACE_HERE);
    }

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    second_processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
    data_after = this->data.load(std::memory_order_relaxed, CXXTRACE_HERE);
    this->rseq.end_preemptable(CXXTRACE_HERE);

    if (is_data_stored) {
      if (data_after != 42) {
        // An acquire fence did not occur on this thread.
        if (first_processor_id == second_processor_id) {
          // Either of the following happened:
          //
          // * This thread stayed on the same processor across both critical
          //   sections. In this case, no acquire fence is implied, thus
          //   data_after==0 is valid.
          // * Between the critical sections, this thread switched to a
          //   different processor then back to the original processor.
          //   Switching processors implies an acquire fence, thus data_after==0
          //   is invalid.
          //
          // We can't tell which scenario occurred, so assume it could be the
          // first.
          CXXTRACE_ASSERT(data_after == 0);
          std::fprintf(stderr, "result: consumer had no acquire fence\n");
        } else {
          // This thread switched to a different processor. Switching processors
          // implies an acquire fence, thus data_after==0 is invalid.
          CXXTRACE_ASSERT(false);
        }
      }
    }

    return;

  preempted:;
  }

  auto run_producer_thread() -> void
  {
    this->data.store(42, std::memory_order_relaxed, CXXTRACE_HERE);
    this->is_data_stored.store(true, std::memory_order_release, CXXTRACE_HERE);
  }

  auto tear_down() -> void {}

  sync::atomic<bool> is_data_stored{ false };
  sync::atomic<int> data{ 0 };
};

// NOTE(strager): This test is a manual test. Ensure the test prints the
// following at least one time:
// * result: producer had no release fence
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class test_critical_section_does_not_force_release_fence
  : public rseq_scheduler_test_base
{
public:
  static constexpr auto force_release_fence = false;

  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    switch (thread_index) {
      case 0:
        this->run_consumer_thread();
        break;
      case 1:
        this->run_producer_thread();
        break;
      default:
        CXXTRACE_ASSERT(false);
        break;
    }
  }

  auto run_producer_thread() -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->data.store(42, std::memory_order_relaxed, CXXTRACE_HERE);
    this->rseq.end_preemptable(CXXTRACE_HERE);

    if (force_release_fence) {
      sync::atomic_thread_fence(std::memory_order_release, CXXTRACE_HERE);
    }

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    this->is_data_stored.store(true, std::memory_order_relaxed, CXXTRACE_HERE);
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
  }

  auto run_consumer_thread() -> void
  {
    auto is_data_stored =
      this->is_data_stored.load(std::memory_order_acquire, CXXTRACE_HERE);
    auto data_after = this->data.load(std::memory_order_relaxed, CXXTRACE_HERE);

    if (is_data_stored) {
      if (data_after != 42) {
        // An release fence did not occur on the other thread.
        //
        // One of the following happened:
        //
        // * The other thread stayed on the same processor across both critical
        //   sections. In this case, no release fence is implied, thus
        //   data_after==0 is valid.
        // * The other thread switched to a different processor between critical
        //   sections. Switching processors implies a release fence, thus
        //   data_after==0 is invalid.
        // * Between the critical sections, the other thread switched to a
        //   different processor then back to its original processor. Switching
        //   processors implies a release fence, thus data_after==0 is invalid.
        //
        // We can't tell which scenario occurred, so assume it could be the
        // first.
        CXXTRACE_ASSERT(data_after == 0);
        std::fprintf(stderr, "result: producer had no release fence\n");
      }
    }
  }

  auto tear_down() -> void {}

  sync::atomic<bool> is_data_stored{ false };
  sync::atomic<int> data{ 0 };
};

class test_preemption_implies_release_fence_for_same_processor
  : public rseq_scheduler_test_base
{
public:
  static constexpr auto force_release_fence = false;

  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    switch (thread_index) {
      case 0:
        this->run_consumer_thread();
        break;
      case 1:
        this->run_producer_thread();
        break;
      default:
        CXXTRACE_ASSERT(false);
        break;
    }
  }

  auto run_producer_thread() -> void
  {
    volatile auto did_store_data = false;
    volatile cxxtrace::detail::processor_id processor_id;

    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);
      this->rseq.allow_preempt(CXXTRACE_HERE);
      this->data.store(42, CXXTRACE_HERE);
      did_store_data = true;
      this->rseq.allow_preempt(CXXTRACE_HERE);
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:
    if (force_release_fence) {
      sync::atomic_thread_fence(std::memory_order_release, CXXTRACE_HERE);
    }
    if (did_store_data) {
      this->producer_processor_id.store(
        processor_id, std::memory_order_relaxed, CXXTRACE_HERE);
    }
  }

  auto run_consumer_thread() -> void
  {
    auto producer_processor_id = this->producer_processor_id.load(
      std::memory_order_acquire, CXXTRACE_HERE);
    if (producer_processor_id == this->invalid_processor_id) {
      // The producer thread hasn't published yet.
      return;
    }
    if (this->rseq.get_current_processor_id(CXXTRACE_HERE) !=
        producer_processor_id) {
      // The producer thread's critical section and the consumer thread possibly
      // never executed on the same processor. No release fence is implied on
      // the producer thread.
      return;
    }
    // The same processor executed the producer thread's critical section and
    // the consumer thread after loading producer_processor_id. A release fence
    // is implied on the producer thread.
    auto data_after = this->data.load(CXXTRACE_HERE);
    CXXTRACE_ASSERT(data_after == 42);
  }

  auto tear_down() -> void {}

  static constexpr auto invalid_processor_id =
    ~cxxtrace::detail::processor_id{ 0 };
  sync::atomic<cxxtrace::detail::processor_id> producer_processor_id{
    invalid_processor_id
  };
  sync::nonatomic<int> data{ 0 };
};

class test_getting_processor_id_implies_acquire_fence_for_same_processor
  : public rseq_scheduler_test_base
{
public:
  static constexpr auto force_acquire_fence = false;

  using rseq_scheduler_test_base::rseq_scheduler_test_base;

  auto run_thread(int thread_index) -> void
  {
    switch (thread_index) {
      case 0:
        this->run_consumer_thread();
        break;
      case 1:
        this->run_producer_thread();
        break;
      default:
        CXXTRACE_ASSERT(false);
        break;
    }
  }

  auto run_producer_thread() -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(this->rseq, preempted);
    {
      auto processor_id = this->rseq.get_current_processor_id(CXXTRACE_HERE);
      CXXTRACE_ASSERT(processor_id != this->invalid_processor_id);
      this->rseq.allow_preempt(CXXTRACE_HERE);
      this->data.store(42, CXXTRACE_HERE);
      this->rseq.allow_preempt(CXXTRACE_HERE);
      this->producer_processor_id.store(
        processor_id, std::memory_order_release, CXXTRACE_HERE);
    }
    this->rseq.end_preemptable(CXXTRACE_HERE);
    return;

  preempted:;
  }

  auto run_consumer_thread() -> void
  {
    auto producer_processor_id = this->producer_processor_id.load(
      std::memory_order_relaxed, CXXTRACE_HERE);
    if (producer_processor_id == this->invalid_processor_id) {
      // The producer thread hasn't published yet.
      return;
    }
    if (this->rseq.get_current_processor_id(CXXTRACE_HERE) !=
        producer_processor_id) {
      // The producer thread's critical section and the consumer thread possibly
      // never executed on the same processor. No acquire fence is implied on
      // the consumer thread.
      return;
    }
    if (force_acquire_fence) {
      sync::atomic_thread_fence(std::memory_order_acquire, CXXTRACE_HERE);
    }
    // The same processor executed the producer thread's critical section and
    // the consumer thread after loading producer_processor_id. An acquire fence
    // is implied on the consumer thread.
    auto data_after = this->data.load(CXXTRACE_HERE);
    CXXTRACE_ASSERT(data_after == 42);
  }

  auto tear_down() -> void {}

  static constexpr auto invalid_processor_id =
    ~cxxtrace::detail::processor_id{ 0 };
  sync::atomic<cxxtrace::detail::processor_id> producer_processor_id{
    invalid_processor_id
  };
  sync::nonatomic<int> data{ 0 };
};

auto
register_concurrency_tests() -> void
{
  register_concurrency_test<test_allow_preempt_call_is_preempted_sometimes>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_allow_preempt_call_is_preempted_sometimes_in_multiple_critical_sections>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_after_critical_section>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_after_aborting_critical_section>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_before_critical_section>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<test_preemptable_state_resets_between_iterations>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_between_critical_sections>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_allow_preempt_calls_on_different_threads_are_preempted_independently>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<
    test_current_processor_id_is_constant_within_critical_section>(
    1, concurrency_test_depth::full, 1);
  register_concurrency_test<
    test_threads_have_different_processor_ids_within_critical_section>(
    test_threads_have_different_processor_ids_within_critical_section::
      thread_count,
    concurrency_test_depth::full,
    test_threads_have_different_processor_ids_within_critical_section::
      thread_count);
  register_concurrency_test<
    test_threads_outside_critical_sections_have_different_processor_ids_than_threads_inside_critical_sections>(
    test_threads_outside_critical_sections_have_different_processor_ids_than_threads_inside_critical_sections::
      thread_count,
    concurrency_test_depth::full,
    test_threads_outside_critical_sections_have_different_processor_ids_than_threads_inside_critical_sections::
      thread_count);
  register_concurrency_test<
    test_preempted_thread_might_share_processor_with_another_thread_in_critical_section>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<
    test_thread_might_change_processor_when_entering_critical_section>(
    1, concurrency_test_depth::full, 2);
  register_concurrency_test<
    test_thread_sees_stores_by_previous_thread_on_processor>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<test_critical_section_does_not_force_acquire_fence>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<test_critical_section_does_not_force_release_fence>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<
    test_preemption_implies_release_fence_for_same_processor>(
    2, concurrency_test_depth::full, 2);
  register_concurrency_test<
    test_getting_processor_id_implies_acquire_fence_for_same_processor>(
    2, concurrency_test_depth::full, 2);
}
}
