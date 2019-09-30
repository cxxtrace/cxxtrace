#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "thread_local_var.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/string.h>
#include <ostream>
#include <setjmp.h>
#include <string>

#if CXXTRACE_ENABLE_CDSCHECKER
#include "cdschecker_exhaustive_rng.h"
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include <cstdlib>
#endif

#if CXXTRACE_ENABLE_RELACY
#include <relacy/context.hpp>
#include <relacy/context_base.hpp>
#endif

namespace cxxtrace_test {
namespace {
class rseq_scheduler
{
public:
  static auto allow_preempt(cxxtrace::detail::debug_source_location caller)
    -> void
  {
    auto preempt_label = thread_state_->preempt_label;
    auto in_critical_section = !!preempt_label;
    if (in_critical_section) {
      auto should_preempt = concurrency_rng_next_integer_0(2) == 0;
      if (should_preempt) {
        cxxtrace_test::concurrency_log(
          [&](std::ostream& out) { out << "goto " << preempt_label; }, caller);
        ::longjmp(preempt_target_jmp_buf(), 1);
      }
    }
  }

  static auto end_preemptable(cxxtrace::detail::debug_source_location caller)
    -> void
  {
    cxxtrace_test::concurrency_log(
      [](std::ostream& out) { out << "CXXTRACE_END_PREEMPTABLE"; }, caller);
    exit_critical_section(caller);
  }

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  static auto in_critical_section(
    cxxtrace::detail::debug_source_location) noexcept -> bool
  {
    return thread_state_->preempt_label;
  }

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  static auto enter_critical_section(
    cxxtrace::czstring preempt_label,
    cxxtrace::detail::debug_source_location) noexcept -> void
  {
    thread_state_->preempt_label = preempt_label;
  }

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  static auto exit_critical_section(
    cxxtrace::detail::debug_source_location) noexcept -> void
  {
    thread_state_->preempt_label = nullptr;
  }

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  static auto preempt_target_jmp_buf() noexcept -> ::jmp_buf&
  {
    return thread_state_->preempt_target;
  }

private:
  struct thread_state
  {
    cxxtrace::czstring preempt_label;
    ::jmp_buf preempt_target;
  };
  static inline thread_local_var<thread_state> thread_state_{};
};

#define CXXTRACE_BEGIN_PREEMPTABLE(preempt_label)                              \
  do {                                                                         \
    ::cxxtrace_test::concurrency_log(                                          \
      [](::std::ostream& out) {                                                \
        out << "CXXTRACE_BEGIN_PREEMPTABLE(" << #preempt_label << ')';         \
      },                                                                       \
      CXXTRACE_HERE);                                                          \
    if (::setjmp(::cxxtrace_test::rseq_scheduler::preempt_target_jmp_buf()) != \
        0) {                                                                   \
      ::cxxtrace_test::concurrency_log(                                        \
        [](::std::ostream& out) {                                              \
          out << "note: CXXTRACE_BEGIN_PREEMPTABLE(" << #preempt_label         \
              << ") was here";                                                 \
        },                                                                     \
        CXXTRACE_HERE);                                                        \
      ::cxxtrace_test::rseq_scheduler::exit_critical_section(CXXTRACE_HERE);   \
      goto preempt_label;                                                      \
    }                                                                          \
    ::cxxtrace_test::rseq_scheduler::enter_critical_section(#preempt_label,    \
                                                            CXXTRACE_HERE);    \
  } while (false)
}

// NOTE(strager): This test is a manual test. Ensure the test prints 'result:
// preempted' and 'result: not preempted' at least one time each.
// TODO(strager): Extend the test framework to make writing an assertion
// possible, automating this test.
class test_allow_preempt_call_is_preempted_sometimes
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_BEGIN_PREEMPTABLE(preempted);

    this->before_allow_preempt = true;
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
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
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    this->critical_section(0);
    this->critical_section(1);
  }

  auto critical_section(int index) -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(preempted);
    this->before_allow_preempt[index] = true;
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
    this->after_allow_preempt[index] = true;
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
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_BEGIN_PREEMPTABLE(preempted);
    rseq_scheduler::end_preemptable(CXXTRACE_HERE);

    this->before_allow_preempt = true;
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
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
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_BEGIN_PREEMPTABLE(preempted);
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
    rseq_scheduler::end_preemptable(CXXTRACE_HERE);
    return;

  preempted:
    this->preempted_count += 1;
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->preempted_count == 0 || this->preempted_count == 1);
  }

  int preempted_count{ 0 };
};

class test_allow_preempt_call_never_preempts_before_critical_section
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    this->before_allow_preempt = true;
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
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
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    CXXTRACE_ASSERT(!rseq_scheduler::in_critical_section(CXXTRACE_HERE));

    // Coerce model checkers into executing another iteration.
    concurrency_rng_next_integer_0(2);

    CXXTRACE_BEGIN_PREEMPTABLE(preempted);
    // Avoid calling end_preemptable so we exit this iteration with
    // in_critical_section()==true.
    CXXTRACE_ASSERT(rseq_scheduler::in_critical_section(CXXTRACE_HERE));
    return;

  preempted:
    CXXTRACE_ASSERT(false);
  }

  auto tear_down() -> void {}
};

class test_allow_preempt_call_never_preempts_between_critical_sections
{
public:
  auto run_thread(int thread_index) -> void
  {
    CXXTRACE_ASSERT(thread_index == 0);

    this->critical_section(0);
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
    this->critical_section(1);
  }

  auto critical_section(int index) -> void
  {
    CXXTRACE_BEGIN_PREEMPTABLE(preempted);
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
    this->not_preempted_count[index] += 1;
    rseq_scheduler::end_preemptable(CXXTRACE_HERE);
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
{
public:
  auto run_thread(int thread_index) -> void
  {
    assert(thread_index == 0 || thread_index == 1);
    CXXTRACE_BEGIN_PREEMPTABLE(preempted);
    this->before_allow_preempt[thread_index] = true;
    rseq_scheduler::allow_preempt(CXXTRACE_HERE);
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

auto
register_concurrency_tests() -> void
{
  register_concurrency_test<test_allow_preempt_call_is_preempted_sometimes>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_allow_preempt_call_is_preempted_sometimes_in_multiple_critical_sections>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_after_critical_section>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_after_aborting_critical_section>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_before_critical_section>(
    1, concurrency_test_depth::full);
  register_concurrency_test<test_preemptable_state_resets_between_iterations>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_allow_preempt_call_never_preempts_between_critical_sections>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_allow_preempt_calls_on_different_threads_are_preempted_independently>(
    2, concurrency_test_depth::full);
}
}
