#ifndef CXXTRACE_TEST_RSEQ_SCHEDULER_H
#define CXXTRACE_TEST_RSEQ_SCHEDULER_H

#include "memory_resource.h"
#include "synchronization.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <cxxtrace/string.h>
#include <experimental/vector>
#include <functional>
#include <mutex>
#include <setjmp.h>
#include <type_traits>
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
//   rseq_scheduler<concurrency_test_synchronization> my_rseq{};
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
//   //
//   // The CXXTRACE_BEGIN_PREEMPTABLE macro introduces a scope which must be
//   // terminated by CXXTRACE_END_PREEMPTABLE. In other words,
//   // CXXTRACE_BEGIN_PREEMPTABLE contains a dangling {, and
//   // CXXTRACE_END_PREEMPTABLE contains a dangling }. This scope has several
//   // consequences, including the following:
//   //
//   // * Automatic variables declared within the critical section are
//   //   inaccessible outside the critical section.
//   // * Automatic variables declared within the critical section are
//   //   destructed before exiting the critical section.
//   // * Each call to CXXTRACE_BEGIN_PREEMPTABLE must preceed exactly one
//   //   corresponding call to CXXTRACE_END_PREEMPTABLE in the same function.
//   //   CXXTRACE_END_PREEMPTABLE must be called in the same scope which
//   //   CXXTRACE_BEGIN_PREEMPTABLE was called in (i.e. matching
//   //   CXXTRACE_BEGIN_PREEMPTABLE and CXXTRACE_END_PREEMPTABLE calls must
//   //   have the same indentation).
//   //
//   // Critical sections must not be nested in source code.
//   //
//   // Critical sections must not be nested in execution order.
//   //
//   // A call to CXXTRACE_BEGIN_PREEMPTABLE does not require a trailing
//   // semicolon.
//   CXXTRACE_BEGIN_PREEMPTABLE(my_rseq, preempt_goto_label)
//
//     // Determine which per-processor data your algorithm should modify.
//     auto processor_id = my_rseq.get_current_processor_id(CXXTRACE_HERE);
//     assert(processor_id < my_data.size());
//     auto& data = my_data[processor_id];
//
//     // allow_preemptable either does nothing or jumps to abort_ip
//     // (preempt_goto_label). Sprinkle calls to allow_preemptable throughout
//     // your algorithm (ideally between each CPU instruction) to model
//     // arbitrary thread preemptions.
//     rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//     /* (Do some work.) */
//
//     // Communicate with the preempt handler with volatile automatic
//     // variables.
//     did_phase_1 = true;
//
//     rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//     rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//     /* (Do more work.) */
//
//     rseq_scheduler::allow_preemptable(CXXTRACE_HERE);
//
//     /* (Publish your data before exiting the critical section.) */
//
//   // CXXTRACE_END_PREEMPTABLE exits a critical section. Calling
//   // CXXTRACE_END_PREEMPTABLE is like executing the instruction at
//   // rseq_cs::post_commit_ip. end_preemptable must be called before returning
//   // from the function which called CXXTRACE_BEGIN_PREEMPTABLE.
//   //
//   // CXXTRACE_END_PREEMPTABLE does not imply any memory ordering.
//   //
//   // The CXXTRACE_END_PREEMPTABLE macro closes the scope opened by
//   // CXXTRACE_BEGIN_PREEMPTABLE. In other words, CXXTRACE_END_PREEMPTABLE
//   // contains a dangling }, corresponding to a dangling { in
//   // CXXTRACE_BEGIN_PREEMPTABLE. Each call to CXXTRACE_END_PREEMPTABLE must
//   // have exactly one preceeding call to CXXTRACE_BEGIN_PREEMPTABLE in the
//   // same function.
//   //
//   // A call to CXXTRACE_END_PREEMPTABLE does not require a trailing
//   // semicolon.
//   CXXTRACE_END_PREEMPTABLE(my_rseq, preempt_goto_label);
//
//   // Outside a critical section, allow_preemptable does nothing. In other
//   // words, after calling CXXTRACE_END_PREEMPTABLE, preempt_goto_label is
//   // forgotten and will not be jumped to.
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
template<class Sync>
class rseq_scheduler
{
private:
  template<class T>
  using atomic = typename Sync::template atomic<T>;
  using debug_source_location = typename Sync::debug_source_location;
  using relaxed_semaphore = typename Sync::relaxed_semaphore;
  template<class T>
  using thread_local_var = typename Sync::template thread_local_var<T>;

public:
  class disable_preemption_guard
  {
  public:
    disable_preemption_guard(const disable_preemption_guard&) = delete;
    disable_preemption_guard& operator=(const disable_preemption_guard&) =
      delete;

    disable_preemption_guard(disable_preemption_guard&&) = delete;
    disable_preemption_guard& operator=(disable_preemption_guard&&) = delete;

    ~disable_preemption_guard();

  private:
    explicit disable_preemption_guard(rseq_scheduler*, debug_source_location);

    rseq_scheduler* scheduler_;
    debug_source_location caller_;

    friend class rseq_scheduler;
  };

  static auto get() -> rseq_scheduler*;

  explicit rseq_scheduler(int processor_count);
  ~rseq_scheduler();

  auto get_current_processor_id(debug_source_location) noexcept
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
  // Calling allow_preempt immediately before calling CXXTRACE_END_PREEMPTABLE
  // is likely an error in your algorithm.
  //
  // [1] See set_preempt_callback for optional steps performed before exiting
  //     the critical section.
  auto allow_preempt(debug_source_location) -> void;

  [[nodiscard]] auto disable_preemption(debug_source_location)
    -> disable_preemption_guard;

  // This function exists for assertions only. Do not use the result of this
  // function to influence your algorithm.
  auto in_critical_section() noexcept -> bool;

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
                              debug_source_location) noexcept -> void;

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  auto finish_preempt(debug_source_location) noexcept -> void;

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  auto preempt_target_jmp_buf() noexcept -> ::jmp_buf&;

  // This function is private to rseq_scheduler. It is visible only so it can be
  // used in macros.
  auto end_preemptable(debug_source_location) -> void;

private:
  struct processor;
  struct thread_state;

  auto exit_critical_section(debug_source_location) noexcept -> void;

  auto get_any_unused_processor_id(debug_source_location)
    -> cxxtrace::detail::processor_id;
  auto mark_processor_id_as_unused(cxxtrace::detail::processor_id,
                                   debug_source_location) -> void;
  auto take_unused_processor_id(debug_source_location)
    -> cxxtrace::detail::processor_id;
  auto wait_for_unused_processor(debug_source_location) -> void;

  auto enable_preemption(debug_source_location) -> void;

  auto current_thread_state() -> thread_state&;

  static inline std::atomic<rseq_scheduler*> instance_{ nullptr };

  std::array<std::byte, 1024> processors_storage_;
  monotonic_buffer_resource processors_memory_;

  // processor_reservation_mutex_ protects processor::in_use (in processors_).
  //
  // NOTE(strager): We use a real mutex (and not a modelled mutex) for two
  // reasons:
  //
  // * We do not want to interfere with the memory order of the user's
  //   algorithm.
  // * Using a modelled mutex adds undesired performance overhead.
  std::mutex processor_reservation_mutex_;
  std::experimental::pmr::vector<processor> processors_;

  // thread_runnable_ is awaited when all processors are in use.
  // thread_runnable_ is posted when a processor becomes unused.
  relaxed_semaphore thread_runnable_;

  // thread_states_storage_ holds an object of type
  // cxxtrace_test::thread_local_var<thread_state>. std::aligned_storage is used
  // to keep most of the guts in rseq_scheduler.cpp (i.e. outside this header
  // file).
  std::aligned_storage_t<1024> thread_states_storage_;

  friend class disable_preemption_guard;
};

extern template class rseq_scheduler<concurrency_test_synchronization>;

// TODO(strager): Should CXXTRACE_BEGIN_PREEMPTABLE implicitly call
// allow_preempt?
#define CXXTRACE_BEGIN_PREEMPTABLE(scheduler, preempt_label)                   \
  { /* Introduce a scope to be closed by CXXTRACE_END_PREEMPTABLE. */          \
    {                                                                          \
      auto& _cxxtrace_scheduler = (scheduler);                                 \
      if (setjmp(_cxxtrace_scheduler.preempt_target_jmp_buf()) != 0) {         \
        _cxxtrace_scheduler.finish_preempt(CXXTRACE_HERE);                     \
        goto preempt_label;                                                    \
      }                                                                        \
      _cxxtrace_scheduler.enter_critical_section(#preempt_label,               \
                                                 CXXTRACE_HERE);               \
    }

#define CXXTRACE_END_PREEMPTABLE(scheduler, preempt_label)                     \
  } /* Terminate the scope introduced by CXXTRACE_BEGIN_PREEMPTABLE. */        \
  {                                                                            \
    auto& _cxxtrace_scheduler = (scheduler);                                   \
    _cxxtrace_scheduler.end_preemptable(CXXTRACE_HERE);                        \
  }

#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_CONCURRENCY_STRESS ||        \
  CXXTRACE_ENABLE_RELACY
class rseq_scheduler_test_base
{
public:
  explicit rseq_scheduler_test_base(int processor_count)
    : rseq{ processor_count }
  {}

  rseq_scheduler<concurrency_test_synchronization> rseq;
};
#endif
}

#endif
