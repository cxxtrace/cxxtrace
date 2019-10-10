#include "rseq_scheduler.h"
#include "cxxtrace_concurrency_test.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <ostream>
#include <string>
#include <utility>
// IWYU pragma: no_include "relacy_thread_local_var.h"

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include <cxxtrace/detail/real_synchronization.h>
#endif
// IWYU pragma: no_forward_declare cxxtrace::detail::real_synchronization

#if CXXTRACE_ENABLE_RELACY
#include "relacy_synchronization.h"
#include <relacy/context.hpp>
#include <relacy/context_base.hpp>
#endif
// IWYU pragma: no_forward_declare cxxtrace_test::relacy_synchronization

#if CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
#error                                                                         \
  "rseq_scheduler is not supported with CDSChecker because concurrency_rng_next_integer_0 is not implemented"
#endif

namespace cxxtrace_test {
namespace {
template<class Sync>
auto
get_test_thread_count() noexcept -> int = delete;

#if CXXTRACE_ENABLE_CDSCHECKER
template<>
auto
get_test_thread_count<cdschecker_synchronization>() noexcept -> int
{
  // TODO(strager): Create an API to query the number of test threads.
  return 3;
}
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
template<>
auto
get_test_thread_count<cxxtrace::detail::real_synchronization>() noexcept -> int
{
  // TODO(strager): Create an API to query the number of test threads.
  return 3;
}
#endif

#if CXXTRACE_ENABLE_RELACY
template<>
auto
get_test_thread_count<relacy_synchronization>() noexcept -> int
{
  return rl::ctx().get_thread_count();
}
#endif
}

template<class Sync>
rseq_scheduler<Sync>::rseq_scheduler()
{
  // HACK(strager): Create at least two processors to make
  // test_thread_might_change_processor_when_entering_critical_section pass.
  // HACK(strager): Create at least one processor per thread to allow all
  // threads to successfully enter a critical section without blocking.
  auto thread_count = get_test_thread_count<Sync>();
  this->processor_id_count_ = std::max(thread_count, 2);
}

template<class Sync>
auto
rseq_scheduler<Sync>::get_current_processor_id(
  debug_source_location caller) noexcept -> cxxtrace::detail::processor_id
{
  const auto& state = *this->thread_state_;
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
  auto& state = *thread_state_;
  if (state.in_critical_section()) {
    auto should_preempt = concurrency_rng_next_integer_0(2) == 1;
    if (should_preempt) {
      state.preempt(caller);
    }
  }
}

template<class Sync>
[[noreturn]] auto
rseq_scheduler<Sync>::thread_state::preempt(debug_source_location caller)
  -> void
{
  assert(this->in_critical_section());
  cxxtrace_test::concurrency_log(
    [&](std::ostream& out) { out << "goto " << this->preempt_label; }, caller);
  auto* preempt_callback = this->preempt_callback;
  if (preempt_callback) {
    (*preempt_callback)();
  }
  ::longjmp(this->preempt_target, 1);
}

template<class Sync>
auto
rseq_scheduler<Sync>::set_preempt_callback(std::function<void()>&& on_preempt)
  -> void
{
  auto& state = *thread_state_;
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
  return thread_state_->in_critical_section();
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
  auto& state = *this->thread_state_;
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
  auto& state = *this->thread_state_;
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
  return this->thread_state_->preempt_target;
}

template<class Sync>
auto
rseq_scheduler<Sync>::thread_state::in_critical_section() const noexcept -> bool
{
  return !!this->preempt_label;
}

template<class Sync>
auto
rseq_scheduler<Sync>::exit_critical_section(
  debug_source_location caller) noexcept -> void
{
  auto& state = *this->thread_state_;
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
    for (auto processor_id = 0; processor_id < this->processor_id_count_;
         ++processor_id) {
      if (!this->processors_[processor_id].in_use) {
        count += 1;
      }
    }
    return count;
  };

  auto get_unused_processor = [this](int unused_processors_to_skip,
                                     const unique_lock&) -> processor* {
    for (auto processor_id = 0; processor_id < this->processor_id_count_;
         ++processor_id) {
      auto& processor = this->processors_[processor_id];
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
    auto unused_processor_count = count_unused_processors(lock);
    if (unused_processor_count == 0) {
      lock.unlock();
      CXXTRACE_ASSERT(false);
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
  auto& processor = this->processors_[processor_id];
  processor.release_baton(caller);
  {
    auto lock = std::unique_lock{ this->processor_reservation_mutex_ };
    CXXTRACE_ASSERT(processor.in_use);
    processor.in_use = false;
  }
}

template<class Sync>
auto
rseq_scheduler<Sync>::take_unused_processor_id(debug_source_location caller)
  -> cxxtrace::detail::processor_id
{
  auto lock = std::unique_lock{ this->processor_reservation_mutex_ };
  for (auto processor_id = 0; processor_id < this->processor_id_count_;
       ++processor_id) {
    auto& processor = this->processors_[processor_id];
    if (!processor.in_use) {
      processor.in_use = true;
      lock.unlock();
      processor.maybe_acquire_baton(caller);
      return processor_id;
    }
  }

  lock.unlock();
  CXXTRACE_ASSERT(false);
  __builtin_unreachable();
}

template<class Sync>
auto
rseq_scheduler<Sync>::processor::maybe_acquire_baton(
  debug_source_location caller) -> void
{
  if (this->has_baton) {
    [[maybe_unused]] auto baton =
      this->baton.load(std::memory_order_seq_cst, caller);
  }
}

template<class Sync>
auto
rseq_scheduler<Sync>::processor::release_baton(debug_source_location caller)
  -> void
{
  this->baton.store(true, std::memory_order_seq_cst, caller);
  this->has_baton = true;
}

template class rseq_scheduler<concurrency_test_synchronization>;
}
