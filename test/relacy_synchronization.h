#ifndef CXXTRACE_TEST_RELACY_SYNCHRONIZATION_H
#define CXXTRACE_TEST_RELACY_SYNCHRONIZATION_H

// IWYU pragma: no_include <ostream>

#if CXXTRACE_ENABLE_RELACY
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/atomic_base.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>
#include <cxxtrace/detail/workarounds.h>
#include <exception>
#include <pthread.h>
#include <system_error>
#include <type_traits>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdynamic-exception-spec")
CXXTRACE_WARNING_IGNORE_CLANG("-Wextra-semi")
CXXTRACE_WARNING_IGNORE_CLANG("-Winline-new-delete")
CXXTRACE_WARNING_IGNORE_CLANG("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_GCC("-Wsized-deallocation")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")

// clang-format off
#include <relacy/thread_local_ctx.hpp>
// clang-format on
#include <relacy/atomic.hpp>
#include <relacy/atomic_fence.hpp>
#include <relacy/memory_order.hpp>
#include <relacy/thread_local.hpp>
#include <relacy/var.hpp>

CXXTRACE_WARNING_POP
#endif

namespace cxxtrace_test {
#if CXXTRACE_ENABLE_RELACY
class relacy_synchronization
{
public:
  using debug_source_location = cxxtrace::detail::debug_source_location;

  template<class T>
  class atomic;

  template<class T>
  class nonatomic;

  template<class T>
  class thread_local_var;

  static auto atomic_thread_fence(std::memory_order,
                                  debug_source_location) noexcept -> void;

private:
  static auto relacy_cast(std::memory_order memory_order) noexcept
    -> rl::memory_order;
};

inline auto
relacy_synchronization::relacy_cast(std::memory_order memory_order) noexcept
  -> rl::memory_order
{
  switch (memory_order) {
    case std::memory_order_relaxed:
      return rl::mo_relaxed;
    case std::memory_order_consume:
      return rl::mo_consume;
    case std::memory_order_acquire:
      return rl::mo_acquire;
    case std::memory_order_release:
      return rl::mo_release;
    case std::memory_order_acq_rel:
      return rl::mo_acq_rel;
    case std::memory_order_seq_cst:
      return rl::mo_seq_cst;
  }
  __builtin_unreachable();
}

template<class T>
class relacy_synchronization::atomic
  : public cxxtrace::detail::atomic_base<T, atomic<T>>
{
private:
  using base = cxxtrace::detail::atomic_base<T, atomic<T>>;

public:
  using base::compare_exchange_strong;
  using base::fetch_add;
  using base::load;
  using base::store;

  explicit atomic() /* data uninitialized */ = default;

  /* implicit */ atomic(T value) noexcept
    : data{ value }
  {}

  auto compare_exchange_strong(T& expected,
                               T desired,
                               std::memory_order success_order,
                               std::memory_order failure_order,
                               debug_source_location caller) noexcept -> bool
  {
    return this->data.compare_exchange_strong(expected,
                                              desired,
                                              relacy_cast(success_order),
                                              caller,
                                              relacy_cast(failure_order),
                                              caller);
  }

  auto fetch_add(T addend,
                 std::memory_order memory_order,
                 debug_source_location caller) noexcept -> T
  {
    return this->data.fetch_add(addend, relacy_cast(memory_order), caller);
  }

  auto load(std::memory_order memory_order, debug_source_location caller) const
    noexcept -> T
  {
    return this->data.load(relacy_cast(memory_order), caller);
  }

  auto store(T value,
             std::memory_order memory_order,
             debug_source_location caller) noexcept -> void
  {
    this->data.store(value, relacy_cast(memory_order), caller);
  }

private:
  rl::atomic<T> data /* uninitialized */;
};

template<class T>
class relacy_synchronization::nonatomic
  : public cxxtrace::detail::nonatomic_base
{
public:
  explicit nonatomic() /* data uninitialized */ = default;

  /* implicit */ nonatomic(T value) noexcept
    : data{ value }
  {}

  auto load(debug_source_location caller) const noexcept -> T
  {
    return this->data(caller).load();
  }

  auto store(T value, debug_source_location caller) noexcept -> void
  {
    this->data(caller).store(value);
  }

private:
  rl::var<T> data /* uninitialized */;
};

template<class T>
class relacy_synchronization::thread_local_var
{
public:
  static_assert(std::is_trivial_v<T>);

  auto operator-> () noexcept -> T* { return this->get(); }

  auto operator*() noexcept -> T& { return *this->get(); }

  auto get() noexcept -> T*
  {
    auto thread_index = rl::thread_index();
    // If this assertion fails, increase maximum_thread_count.
    assert(thread_index < slots_.size());
    auto& slot = this->slots_[thread_index];
    if (!this->slot_initialized_.get(CXXTRACE_HERE)) {
      std::memset(&slot, 0, sizeof(slot));
      this->slot_initialized_.set(true, CXXTRACE_HERE);
    }
    return &slot;
  }

private:
  static constexpr auto maximum_thread_count = 3;

  rl::thread_local_var<bool> slot_initialized_{};
  std::array<T, maximum_thread_count> slots_;
};

inline auto
relacy_synchronization::atomic_thread_fence(
  std::memory_order memory_order,
  debug_source_location caller) noexcept -> void
{
  rl::atomic_thread_fence(relacy_cast(memory_order), caller);
}
#endif
}

#endif
