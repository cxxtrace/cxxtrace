#ifndef CXXTRACE_TEST_RSEQ_SCHEDULER_H
#define CXXTRACE_TEST_RSEQ_SCHEDULER_H

#include "synchronization.h"
#include "thread_local_var.h" // IWYU pragma: keep
#include <array>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <cxxtrace/string.h>
#include <functional>
#include <mutex>
#include <setjmp.h>
// IWYU pragma: no_forward_declare cxxtrace_test::cdschecker_thread_local_var
// IWYU pragma: no_forward_declare cxxtrace_test::pthread_thread_local_var
// IWYU pragma: no_forward_declare cxxtrace_test::relacy_thread_local_var
// IWYU pragma: no_include "cdschecker_thread_local_var.h"
// IWYU pragma: no_include "pthread_thread_local_var.h"
// IWYU pragma: no_include "relacy_thread_local_var.h"
// IWYU pragma: no_include <ostream>
// IWYU pragma: no_include <relacy/context_base_impl.hpp>

#if CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
#error                                                                         \
  "rseq_scheduler is not supported with CDSChecker because concurrency_rng_next_integer_0 is not implemented"
#endif

namespace cxxtrace_test {
// Linux restartable sequences [1] implemented for model checkers.
//
// [1] https://www.efficios.com/blog/2019/02/08/linux-restartable-sequences/
//
// Example usage:
//
//   // Declare your per-processor data structure.
//   std::array<per_processor_data, max_processor_count> my_data;
//
//   // Declare a singleton rseq_scheduler object. Share a single rseq_scheduler
//   // object between all of your threads.
//   // Do not declare rseq_scheduler with static storage duration. Call
//   // rseq_scheduler's default constructor each test iteration.
//   rseq_scheduler my_rseq{};
//
//   // If you want to write to an automatic variable within a critical section
//   // and read from that automatic variable in a preempt handler, you must
//   // declare that automatic variable as volatile.
//   //
//   // Non-automatic variables do not need to be marked volatile.
//   //
//   // Automatic variables which are not modified within the critical section
//   // do not need to be marked volatile.
//   //
//   // Automatic variables which are not read by the preempt handler do not
//   // need to be marked volatile.
//   volatile auto did_phase_1 = false;
//
//   // Calling CXXTRACE_BEGIN_PREEMPTABLE is like setting rseq::rseq_cs for the
//   // current thread. CXXTRACE_BEGIN_PREEMPTABLE enters a critical section.
//   // rseq_cs::abort_ip is set to preempt_goto_label.
//   CXXTRACE_BEGIN_PREEMPTABLE(my_rseq, preempt_goto_label);
//
//   // Determine which per-processor data your algorithm should modify.
//   auto processor_id = my_rseq.get_current_processor_id(CXXTRACE_HERE);
//   assert(processor_id < my_data.size());
//   auto& data = my_data[processor_id];
//
//   // allow_preemptable either does nothing or jumps to abort_ip
//   // (preempt_goto_label). Sprinkle calls to allow_preemptable throughout
//   // your algorithm (ideally between each CPU instruction) to model arbitrary
//   // thread preemptions.
//   rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//   /* (Do some work.) */
//
//   // Communicate with the preempt handler with volatile automatic variables.
//   did_phase_1 = true;
//
//   rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//   rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//   /* (Do more work.) */
//
//   rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//   /* (Publish your data before exiting the critical section.) */
//
//   // end_preemptable exits a critical section. Calling end_preemptable is
//   // like executing the instruction at rseq_cs::post_commit_ip.
//   // end_preemptable must be called before returning from the function which
//   // called CXXTRACE_BEGIN_PREEMPTABLE.
//   //
//   // end_preemptable does not imply any memory ordering.
//   my_rseq.end_preemptable(CXXTRACE_HERE);
//
//   // Outside a critical section, allow_preemptable does nothing. In other
//   // words, after calling end_preemptable, preempt_goto_label is forgotten
//   // and will not be jumped to.
//   /* rseq_scheduler::allow_preemptable(CXXTRACE_HERE); */
//
//   // Avoid falling through to preempt_goto_label below.
//   return;
//
//   // allow_preemptable might jump to the preempt_goto_label.
//   // preempt_goto_label is like the target of rseq_cs::abort_ip.
//   preempt_goto_label:
//
//   /* (Do cleanup work. Maybe retry, calling CXXTRACE_BEGIN_PREEMPTABLE
//      again.) */
class rseq_scheduler
{
public:
  explicit rseq_scheduler();

  auto get_current_processor_id(
    cxxtrace::detail::debug_source_location) noexcept
    -> cxxtrace::detail::processor_id;

  // Possibly jump to the label registered with CXXTRACE_BEGIN_PREEMPTABLE.
  //
  // Within a critical section, allow_preempt will non-deterministically do
  // exactly one of the following:
  //
  // * Exit the critical section [1], then execute 'goto preempt_goto_label;',
  //   where preempt_goto_label is the label given to
  //   CXXTRACE_BEGIN_PREEMPTABLE.
  // * Nothing.
  //
  // Sprinkle calls to allow_preempt throughout your algorithm. Ideally, call
  // allow_preempt between each modelled machine code instruction.
  //
  // Calling allow_preempt immediately before calling end_preemptable is likely
  // an error in your algorithm.
  //
  // [1] See set_preempt_callback for optional steps performed before exiting
  //     the critical section.
  static auto allow_preempt(cxxtrace::detail::debug_source_location) -> void;

  // TODO(strager): Convert end_preemptable into a macro to mirror
  // CXXTRACE_BEGIN_PREEMPTABLE.
  // TODO(strager): Require a matching call to end_preemptable for each call to
  // CXXTRACE_BEGIN_PREEMPTABLE.
  auto end_preemptable(cxxtrace::detail::debug_source_location) -> void;

  // This function exists for assertions only. Do not use the result of this
  // function to influence your algorithm.
  static auto in_critical_section() noexcept -> bool;

  // Register a hook when preemption occurs.
  //
  // When allow_preempt decides to preempt the current critical section,
  // allow_preempt will call the given function immediately before exiting the
  // critical section.
  //
  // The function given to set_preempt_callback must not call any methods in
  // rseq_scheduler.
  //
  // set_preempt_callback can be called at most once per critical section.
  //
  // This function exists to test rseq_scheduler only. Do not use the effect of
  // this function to influence your algorithm.
  auto set_preempt_callback(std::function<void()> &&) -> void;

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  auto enter_critical_section(cxxtrace::czstring preempt_label,
                              cxxtrace::detail::debug_source_location) noexcept
    -> void;

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  auto finish_preempt(cxxtrace::detail::debug_source_location) noexcept -> void;

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  auto preempt_target_jmp_buf() noexcept -> ::jmp_buf&;

private:
  struct thread_state
  {
    auto in_critical_section() const noexcept -> bool;

    [[noreturn]] auto preempt(cxxtrace::detail::debug_source_location) -> void;

    // preempt_label is a nullable pointer to a statically-allocated string.
    //
    // If preempt_label is not null, the thread is within a critical section.
    //
    // preempt_label is zero-initialized by thread_local_var.
    cxxtrace::czstring preempt_label;

    // preempt_target and processor_id are valid if and only if
    // in_critical_section().
    ::jmp_buf preempt_target;
    cxxtrace::detail::processor_id processor_id;

    // preempt_callback is a nullable owning pointer to new-allocated memory.
    //
    // preempt_callback is a raw pointer so thread_state can be used with
    // thread_local_var (which requires a trivial type).
    //
    // Invariant: preempt_callback == nullptr if !in_critical_section().
    std::function<void()>* preempt_callback;
  };

  static thread_local_var<thread_state> thread_state_;

  auto exit_critical_section(cxxtrace::detail::debug_source_location) noexcept
    -> void;

  auto get_any_unused_processor_id(cxxtrace::detail::debug_source_location)
    -> cxxtrace::detail::processor_id;
  auto mark_processor_id_as_unused(cxxtrace::detail::processor_id,
                                   cxxtrace::detail::debug_source_location)
    -> void;
  auto take_unused_processor_id(cxxtrace::detail::debug_source_location)
    -> cxxtrace::detail::processor_id;

  struct processor
  {
    auto maybe_acquire_baton(cxxtrace::detail::debug_source_location) -> void;
    auto release_baton(cxxtrace::detail::debug_source_location) -> void;

    bool in_use{ false };

    // Model synchronization when a processor switches threads.
    //
    // If a processor is running thread A, then switches to running thread B,
    // all writes from A should be visible on thread B. maybe_acquire_baton and
    // release_baton use the baton variable to synchronize on thread switch.
    //
    // has_baton can only be accessed for the thread owning this processor.
    // processor_reservation_mutex_ provides synchronization (but not mutual
    // exclusion) for has_baton.
    bool has_baton{ false };
    concurrency_test_synchronization::atomic<bool> baton{ true };
  };

  static constexpr auto maximum_processor_count = 3;

  // processor_reservation_mutex_ protects processor::in_use (in processors_).
  //
  // NOTE(strager): We use a real mutex (and not a modelled mutex) for two
  // reasons:
  //
  // * We do not want to interfere with the memory order of the user's
  //   algorithm.
  // * Using a modelled mutex adds undesired performance overhead.
  std::mutex processor_reservation_mutex_;
  std::array<processor, maximum_processor_count> processors_;
  int processor_id_count_;
};

// TODO(strager): Should CXXTRACE_BEGIN_PREEMPTABLE implicitly call
// allow_preempt?
#define CXXTRACE_BEGIN_PREEMPTABLE(scheduler, preempt_label)                   \
  do {                                                                         \
    ::cxxtrace_test::rseq_scheduler& _cxxtrace_scheduler = (scheduler);        \
    if (setjmp(_cxxtrace_scheduler.preempt_target_jmp_buf()) != 0) {           \
      _cxxtrace_scheduler.finish_preempt(CXXTRACE_HERE);                       \
      goto preempt_label;                                                      \
    }                                                                          \
    _cxxtrace_scheduler.enter_critical_section(#preempt_label, CXXTRACE_HERE); \
  } while (false)

#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_CONCURRENCY_STRESS ||        \
  CXXTRACE_ENABLE_RELACY
class rseq_scheduler_test_base
{
public:
  rseq_scheduler rseq;
};
#endif
}

#endif
