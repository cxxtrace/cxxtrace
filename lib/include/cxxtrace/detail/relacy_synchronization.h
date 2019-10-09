#ifndef CXXTRACE_DETAIL_RELACY_SYNCHRONIZATION_H
#define CXXTRACE_DETAIL_RELACY_SYNCHRONIZATION_H

#if CXXTRACE_ENABLE_RELACY
#include <atomic>
#include <cxxtrace/detail/atomic_base.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>

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

#include <relacy/atomic.hpp>
#include <relacy/atomic_fence.hpp>
#include <relacy/memory_order.hpp>
#include <relacy/var.hpp>

CXXTRACE_WARNING_POP
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_ENABLE_RELACY
inline auto
relacy_cast(std::memory_order memory_order) noexcept -> rl::memory_order
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
class relacy_atomic : public atomic_base<T, relacy_atomic<T>>
{
private:
  using base = atomic_base<T, relacy_atomic<T>>;

public:
  using base::compare_exchange_strong;
  using base::fetch_add;
  using base::load;
  using base::store;

  explicit relacy_atomic() /* data uninitialized */ = default;

  /* implicit */ relacy_atomic(T value) noexcept
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
class relacy_nonatomic : public nonatomic_base
{
public:
  explicit relacy_nonatomic() /* data uninitialized */ = default;

  /* implicit */ relacy_nonatomic(T value) noexcept
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

inline auto
relacy_atomic_thread_fence(std::memory_order memory_order,
                           debug_source_location caller) noexcept -> void
{
  rl::atomic_thread_fence(relacy_cast(memory_order), caller);
}
#endif
}
}

#endif
