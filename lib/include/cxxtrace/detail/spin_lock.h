#ifndef CXXTRACE_DETAIL_SPIN_LOCK_H
#define CXXTRACE_DETAIL_SPIN_LOCK_H

#include <atomic>
#include <cstdint>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/workarounds.h>

namespace cxxtrace {
namespace detail {
class atomic_flag_spin_lock
{
public:
  auto try_lock(debug_source_location caller) noexcept -> bool
  {
    auto was_locked =
      this->is_locked.test_and_set(std::memory_order_acquire, caller);
    return !was_locked;
  }

  auto unlock(debug_source_location caller) noexcept -> void
  {
    this->is_locked.clear(std::memory_order_release, caller);
  }

private:
  atomic_flag is_locked;
};

#if defined(__x86_64__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
template<class LockType>
class x86_bts_spin_lock
{
public:
  auto try_lock(debug_source_location) noexcept -> bool
  {
    auto was_locked = bool{};
    asm("lock bts $0, %[lock]"
        : [lock] "+m"(this->lock), "=@ccc"(was_locked)
        :
        : "cc");
    return !was_locked;
  }

  auto unlock(debug_source_location caller) noexcept -> void
  {
    this->lock.store(0, std::memory_order_release, caller);
  }

private:
  real_atomic<LockType> lock{ 0 };
  static_assert(sizeof(lock) == 1);
};
#endif

#if defined(__x86_64__)
template<class LockType>
class x86_xchg_spin_lock
{
public:
  auto try_lock(debug_source_location caller) noexcept -> bool
  {
    auto was_locked = this->lock.exchange(1, std::memory_order_seq_cst, caller);
    return was_locked == 0;
  }

  auto unlock(debug_source_location caller) noexcept -> void
  {
    this->lock.store(0, std::memory_order_release, caller);
  }

private:
  atomic<LockType> lock{ 0 };
};
#endif

using spin_lock =
#if CXXTRACE_WORK_AROUND_SLOW_ATOMIC_FLAG && defined(__x86_64__) &&            \
  defined(__GCC_ASM_FLAG_OUTPUTS__)
  x86_bts_spin_lock<std::uint8_t>
#elif CXXTRACE_WORK_AROUND_SLOW_ATOMIC_FLAG && defined(__x86_64__)
  x86_xchg_spin_lock<std::uint8_t>
#else
  atomic_flag_spin_lock
#endif
  ;
}
}

#endif
