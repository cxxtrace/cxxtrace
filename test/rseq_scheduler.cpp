#include "cxxtrace_concurrency_test.h"
#include "relacy_synchronization.h"
#include "rseq_scheduler.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <new>
#include <ostream>
#include <string>
#include <utility>
// IWYU pragma: no_include "relacy_thread_local_var.h"

#if CXXTRACE_ENABLE_RELACY
#include <cstdio>
#include <exception>
#include <relacy/context.hpp>
#include <relacy/context_base.hpp>
#include <relacy/sync_var.hpp>
#include <type_traits>
#endif

#if CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
#error                                                                         \
  "rseq_scheduler is not supported with CDSChecker because concurrency_rng_next_integer_0 is not implemented"
#endif

namespace cxxtrace_test {
namespace {
template<class Sync>
class memory_order_baton
{
private:
  template<class T>
  using atomic = typename Sync::template atomic<T>;
  using debug_source_location = typename Sync::debug_source_location;

public:
  auto acquire(debug_source_location caller) -> void
  {
    [[maybe_unused]] auto baton =
      this->baton_.load(std::memory_order_seq_cst, caller);
  }

  auto release(debug_source_location caller) -> void
  {
    this->baton_.store(true, std::memory_order_seq_cst, caller);
  }

private:
  atomic<bool> baton_ /* uninitialized */;
};

#if CXXTRACE_ENABLE_RELACY
template<>
class memory_order_baton<relacy_synchronization>
{
private:
  using debug_source_location = relacy_synchronization::debug_source_location;

  template<class Func>
  auto with_sync_var(Func&& callback) -> auto
  {
#define CASE(thread_count)                                                     \
  case thread_count:                                                           \
    return std::forward<Func>(callback)(&this->sync_var_##thread_count##_);
    switch (this->thread_count_) {
      CASE(1)
      CASE(2)
      CASE(3)
      CASE(4)
      default:
        std::fprintf(stderr,
                     "fatal: with_sync_var not implemented for %d threads\n",
                     this->thread_count_);
        std::terminate();
        break;
    }
#undef CASE
  }

public:
  explicit memory_order_baton()
    : thread_count_{ rl::ctx().get_thread_count() }
  {
    this->with_sync_var([](auto* sync_var) -> void {
      using sync_var_type = std::remove_pointer_t<decltype(sync_var)>;
      new (sync_var) sync_var_type{};
    });
  }

  memory_order_baton(const memory_order_baton&) = delete;
  memory_order_baton& operator=(const memory_order_baton&) = delete;

  memory_order_baton(memory_order_baton&&) = delete;
  memory_order_baton& operator=(memory_order_baton&&) = delete;

  ~memory_order_baton()
  {
    this->with_sync_var([](auto* sync_var) -> void { sync_var->~sync_var(); });
  }

  auto acquire(debug_source_location) -> void { this->acquire_release(); }

  auto release(debug_source_location) -> void { this->acquire_release(); }

private:
  auto acquire_release() -> void
  {
    this->with_sync_var([](auto* sync_var) -> void {
      auto* thread = rl::ctx().threadx_;
      sync_var->acq_rel(thread);
    });
  }

  int thread_count_;
  union
  {
    rl::sync_var<1> sync_var_1_;
    rl::sync_var<2> sync_var_2_;
    rl::sync_var<3> sync_var_3_;
    rl::sync_var<4> sync_var_4_;
  };
};
#endif
}

template<class Sync>
struct rseq_scheduler<Sync>::processor
{
  auto maybe_acquire_baton(debug_source_location caller) -> void
  {
    if (this->has_baton) {
      this->baton.acquire(caller);
    }
  }

  auto release_baton(debug_source_location caller) -> void
  {
    this->baton.release(caller);
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
  memory_order_baton<Sync> baton{};
};

template<class Sync>
struct rseq_scheduler<Sync>::thread_state
{
  auto in_critical_section() const noexcept -> bool
  {
    return !!this->preempt_label;
  }

  auto is_preemption_disabled() const noexcept -> bool
  {
    return this->disable_preemption_count > 0;
  }

  [[noreturn]] auto preempt(debug_source_location caller) -> void
  {
    assert(this->in_critical_section());
    cxxtrace_test::concurrency_log(
      [&](std::ostream& out) {
        out << "goto " << this->preempt_label;
      }, // @@@ reword.
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

  // preempt_target is valid if and only if in_critical_section().
  ::jmp_buf preempt_target;

  // processor_id is valid if and only if (in_critical_section() ||
  // is_preemption_disabled()).
  cxxtrace::detail::processor_id processor_id;

  // disable_preemption_count is a reference count of the number of live
  // disable_preemption_guard objects.
  int disable_preemption_count;

  // preempt_callback is a nullable owning pointer to new-allocated memory.
  //
  // preempt_callback is a raw pointer so thread_state can be used with
  // thread_local_var (which requires a trivial type).
  //
  // Invariant: preempt_callback == nullptr if !in_critical_section().
  std::function<void()>* preempt_callback;
};

template<class Sync>
rseq_scheduler<Sync>::disable_preemption_guard::disable_preemption_guard(
  rseq_scheduler* scheduler,
  debug_source_location caller)
  : scheduler_{ scheduler }
  , caller_{ caller }
{}

template<class Sync>
rseq_scheduler<Sync>::disable_preemption_guard::~disable_preemption_guard()
{
  this->scheduler_->enable_preemption(this->caller_);
}

template<class Sync>
auto
rseq_scheduler<Sync>::get() -> rseq_scheduler*
{
  return instance_.load();
}

template<class Sync>
rseq_scheduler<Sync>::rseq_scheduler(int processor_count)
  : processors_memory_{ this->processors_storage_.data(),
                        this->processors_storage_.size() }
  , processors_{ static_cast<std::size_t>(processor_count),
                 &this->processors_memory_ }
{
  using thread_states_type =
    typename Sync::template thread_local_var<thread_state>;
  static_assert(alignof(decltype(this->thread_states_storage_)) >=
                alignof(thread_states_type));
  static_assert(sizeof(this->thread_states_storage_) >=
                sizeof(thread_states_type));
  new (&this->thread_states_storage_) thread_states_type{};

  instance_.store(this);
}

template<class Sync>
rseq_scheduler<Sync>::~rseq_scheduler()
{
  [[maybe_unused]] auto* old_instance = instance_.exchange(nullptr);
  assert(old_instance == this);

  using thread_states_type =
    typename Sync::template thread_local_var<thread_state>;
  std::launder(
    reinterpret_cast<thread_states_type*>(&this->thread_states_storage_))
    ->~thread_states_type();
}

template<class Sync>
auto
rseq_scheduler<Sync>::get_current_processor_id(
  debug_source_location caller) noexcept -> cxxtrace::detail::processor_id
{
  const auto& state = this->current_thread_state();
  cxxtrace::detail::processor_id processor_id;
  if (state.in_critical_section() || state.is_preemption_disabled()) {
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
  auto& state = this->current_thread_state();
  if (state.is_preemption_disabled()) {
    return;
  }
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
  auto& state = this->current_thread_state();
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
[[nodiscard]] auto
rseq_scheduler<Sync>::disable_preemption(debug_source_location caller)
  -> disable_preemption_guard
{
  ::cxxtrace_test::concurrency_log(
    [&](::std::ostream& out) { out << "rseq_scheduler::disable_preemption"; },
    caller);

  auto& state = this->current_thread_state();
  CXXTRACE_ASSERT(!state.in_critical_section());
  if (!state.is_preemption_disabled()) {
    // TODO(strager): Take a processor lazily to avoid redundant Relacy
    // iterations.
    state.processor_id = this->take_unused_processor_id(caller);
  }
  state.disable_preemption_count += 1;
  return disable_preemption_guard{ this, caller };
}

template<class Sync>
auto
rseq_scheduler<Sync>::enable_preemption(debug_source_location caller) -> void
{
  ::cxxtrace_test::concurrency_log(
    [&](::std::ostream& out) { out << "rseq_scheduler::enable_preemption"; },
    caller);

  auto& state = this->current_thread_state();
  CXXTRACE_ASSERT(!state.in_critical_section());
  CXXTRACE_ASSERT(state.is_preemption_disabled());
  state.disable_preemption_count -= 1;
  if (!state.is_preemption_disabled()) {
    this->mark_processor_id_as_unused(state.processor_id, caller);
  }
}

template<class Sync>
auto
rseq_scheduler<Sync>::in_critical_section() noexcept -> bool
{
  return this->current_thread_state().in_critical_section();
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
  if (!state.is_preemption_disabled()) {
    state.processor_id = this->take_unused_processor_id(caller);
  }
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
  if (!state.is_preemption_disabled()) {
    this->mark_processor_id_as_unused(state.processor_id, caller);
  }
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
  using thread_states_type =
    typename Sync::template thread_local_var<thread_state>;
  auto& thread_states = *std::launder(
    reinterpret_cast<thread_states_type*>(&this->thread_states_storage_));
  return *thread_states;
}

template class rseq_scheduler<concurrency_test_synchronization>;
}
