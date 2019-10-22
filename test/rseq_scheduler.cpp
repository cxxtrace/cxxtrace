#include "cxxtrace_concurrency_test.h"
#include "rseq_scheduler.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <ostream>
#include <string>
#include <utility>
// IWYU pragma: no_include "relacy_thread_local_var.h"

#if CXXTRACE_ENABLE_RELACY
#include <relacy/context.hpp>
#include <relacy/context_base.hpp>
#endif

#if CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
#error                                                                         \
  "rseq_scheduler is not supported with CDSChecker because concurrency_rng_next_integer_0 is not implemented"
#endif

namespace cxxtrace_test {
template<class Sync>
struct rseq_scheduler<Sync>::processor
{
  auto maybe_acquire_baton(debug_source_location caller) -> void
  {
    if (this->has_baton) {
      [[maybe_unused]] auto baton =
        this->baton.load(std::memory_order_seq_cst, caller);
    }
  }

  auto release_baton(debug_source_location caller) -> void
  {
    this->baton.store(true, std::memory_order_seq_cst, caller);
    this->has_baton = true;
  }

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
  atomic<bool> baton{ true };
};

template<class Sync>
struct rseq_scheduler<Sync>::thread_state
{
  auto in_critical_section() const noexcept -> bool
  {
    return !!this->preempt_label;
  }

  [[noreturn]] auto preempt(debug_source_location caller) -> void
  {
    assert(this->in_critical_section());
    cxxtrace_test::concurrency_log(
      [&](std::ostream& out) { out << "goto " << this->preempt_label; },
      caller);
    auto* preempt_callback = this->preempt_callback;
    if (preempt_callback) {
      (*preempt_callback)();
    }
    ::longjmp(this->preempt_target, 1);
  }

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

template<class Sync>
rseq_scheduler<Sync>::rseq_scheduler(int processor_count)
  : processors_{ static_cast<std::size_t>(processor_count) }
{}

template<class Sync>
rseq_scheduler<Sync>::~rseq_scheduler() = default;

template<class Sync>
auto
rseq_scheduler<Sync>::get_current_processor_id(
  debug_source_location caller) noexcept -> cxxtrace::detail::processor_id
{
  const auto& state = this->current_thread_state();
  cxxtrace::detail::processor_id processor_id;
  if (state.in_critical_section()) {
    processor_id = state.processor_id;
  } else {
    processor_id = this->get_any_unused_processor_id(caller);
  }
  concurrency_log(
    [&](std::ostream& out) { out << "processor_id = " << processor_id; },
    caller);
  return processor_id;
}

template<class Sync>
auto
rseq_scheduler<Sync>::allow_preempt(debug_source_location caller) -> void
{
  auto& state = current_thread_state();
  if (state.in_critical_section()) {
    auto should_preempt = concurrency_rng_next_integer_0(2) == 1;
    if (should_preempt) {
      state.preempt(caller);
    }
  }
}

template<class Sync>
auto
rseq_scheduler<Sync>::set_preempt_callback(std::function<void()>&& on_preempt)
  -> void
{
  auto& state = current_thread_state();
  assert(state.in_critical_section());
  assert(!state.preempt_callback);
  state.preempt_callback = new std::function<void()>{ std::move(on_preempt) };
}

template<class Sync>
auto
rseq_scheduler<Sync>::end_preemptable(debug_source_location caller) -> void
{
  cxxtrace_test::concurrency_log(
    [](std::ostream& out) { out << "CXXTRACE_END_PREEMPTABLE"; }, caller);
  this->exit_critical_section(caller);
}

template<class Sync>
auto
rseq_scheduler<Sync>::in_critical_section() noexcept -> bool
{
  return current_thread_state().in_critical_section();
}

template<class Sync>
auto
rseq_scheduler<Sync>::enter_critical_section(
  cxxtrace::czstring preempt_label,
  debug_source_location caller) noexcept -> void
{
  assert(preempt_label);
  ::cxxtrace_test::concurrency_log(
    [&](::std::ostream& out) {
      out << "CXXTRACE_BEGIN_PREEMPTABLE(" << preempt_label << ')';
    },
    caller);
  auto& state = this->current_thread_state();
  CXXTRACE_ASSERT(
    !state.in_critical_section() &&
    "Critical sections cannot be nested, but nesting was detected");
  state.preempt_label = preempt_label;
  state.processor_id = this->take_unused_processor_id(caller);
}

template<class Sync>
auto
rseq_scheduler<Sync>::finish_preempt(debug_source_location caller) noexcept
  -> void
{
  auto& state = this->current_thread_state();
  assert(state.in_critical_section());

  auto preempt_label = state.preempt_label;
  assert(preempt_label);
  ::cxxtrace_test::concurrency_log(
    [&](::std::ostream& out) {
      out << "note: CXXTRACE_BEGIN_PREEMPTABLE(" << preempt_label
          << ") was here";
    },
    caller);
  this->exit_critical_section(caller);
}

template<class Sync>
auto
rseq_scheduler<Sync>::preempt_target_jmp_buf() noexcept -> ::jmp_buf&
{
  return this->current_thread_state().preempt_target;
}

template<class Sync>
auto
rseq_scheduler<Sync>::exit_critical_section(
  debug_source_location caller) noexcept -> void
{
  auto& state = this->current_thread_state();
  state.preempt_label = nullptr;
  delete std::exchange(state.preempt_callback, nullptr);
  this->mark_processor_id_as_unused(state.processor_id, caller);
}

template<class Sync>
auto
rseq_scheduler<Sync>::get_any_unused_processor_id(debug_source_location caller)
  -> cxxtrace::detail::processor_id
{
  using unique_lock =
    std::unique_lock<decltype(this->processor_reservation_mutex_)>;

  auto count_unused_processors = [this](const unique_lock&) -> int {
    auto count = 0;
    for (const auto& processor : this->processors_) {
      if (!processor.in_use) {
        count += 1;
      }
    }
    return count;
  };

  auto get_unused_processor = [this](int unused_processors_to_skip,
                                     const unique_lock&) -> processor* {
    for (auto& processor : this->processors_) {
      if (!processor.in_use) {
        if (unused_processors_to_skip == 0) {
          return &processor;
        }
        unused_processors_to_skip -= 1;
      }
    }
    return nullptr;
  };

  auto get_random_unused_processor = [&]() -> processor* {
    auto lock = unique_lock{ this->processor_reservation_mutex_ };
  retry:
    assert(lock.owns_lock());
    auto unused_processor_count = count_unused_processors(lock);
    if (unused_processor_count == 0) {
      lock.unlock();
      this->wait_for_unused_processor(CXXTRACE_HERE);
      lock.lock();
      goto retry;
    }
    return get_unused_processor(
      concurrency_rng_next_integer_0(unused_processor_count), lock);
  };

  auto* processor = get_random_unused_processor();
  CXXTRACE_ASSERT(processor);
  processor->maybe_acquire_baton(caller);
  return processor - this->processors_.data();
}

template<class Sync>
auto
rseq_scheduler<Sync>::mark_processor_id_as_unused(
  cxxtrace::detail::processor_id processor_id,
  debug_source_location caller) -> void
{
  auto& processor = this->processors_.at(processor_id);
  processor.release_baton(caller);
  {
    auto lock = std::unique_lock{ this->processor_reservation_mutex_ };
    CXXTRACE_ASSERT(processor.in_use);
    processor.in_use = false;
  }
  this->thread_runnable_.signal(CXXTRACE_HERE);
}

template<class Sync>
auto
rseq_scheduler<Sync>::take_unused_processor_id(debug_source_location caller)
  -> cxxtrace::detail::processor_id
{
  auto try_take_unused_processor = [&]() -> processor* {
    auto lock = std::unique_lock{ this->processor_reservation_mutex_ };
    for (auto& processor : this->processors_) {
      if (!processor.in_use) {
        processor.in_use = true;
        return &processor;
      }
    }
    return nullptr;
  };

retry:
  if (auto* processor = try_take_unused_processor()) {
    processor->maybe_acquire_baton(caller);
    return processor - this->processors_.data();
  }
  this->wait_for_unused_processor(CXXTRACE_HERE);
  goto retry;
}

template<class Sync>
auto
rseq_scheduler<Sync>::wait_for_unused_processor(debug_source_location caller)
  -> void
{
  this->thread_runnable_.wait(caller);
}

template<class Sync>
auto
rseq_scheduler<Sync>::current_thread_state() -> thread_state&
{
  static auto state = thread_local_var<thread_state>{};
  return *state;
}

template class rseq_scheduler<concurrency_test_synchronization>;
}
