#ifndef CXXTRACE_MUTEX_H
#define CXXTRACE_MUTEX_H

#include "void_t.h"
#include <atomic>
#include <cstdint>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <type_traits>
#include <utility>

#if CXXTRACE_HAVE_OS_SPIN_LOCK
#include <libkern/OSSpinLockDeprecated.h>
#endif

#if CXXTRACE_HAVE_OS_UNFAIR_LOCK
#include <os/lock.h>
#endif

namespace cxxtrace_test {
template<class Mutex, class = void_t<>>
class wrapped_mutex;

template<class Mutex>
class wrapped_mutex<
  Mutex,
  void_t<decltype(std::declval<Mutex&>().try_lock(
    std::declval<cxxtrace::detail::debug_source_location>()))>>
{
public:
  [[gnu::always_inline]] auto try_lock(
    cxxtrace::detail::debug_source_location caller) noexcept -> bool
  {
    return this->underlying_mutex.try_lock(caller);
  }

  [[gnu::always_inline]] auto unlock(
    cxxtrace::detail::debug_source_location caller) noexcept -> void
  {
    return this->underlying_mutex.unlock(caller);
  }

  Mutex underlying_mutex;
};

template<class Mutex>
class wrapped_mutex<Mutex, void_t<decltype(std::declval<Mutex&>().try_lock())>>
{
public:
  [[gnu::always_inline]] auto try_lock(
    cxxtrace::detail::debug_source_location) noexcept -> bool
  {
    return this->underlying_mutex.try_lock();
  }

  [[gnu::always_inline]] auto unlock(
    cxxtrace::detail::debug_source_location) noexcept -> void
  {
    return this->underlying_mutex.unlock();
  }

  Mutex underlying_mutex;
};

template<class LockType>
class compare_exchange_strong_spin_lock
{
public:
  auto try_lock() -> bool
  {
    auto expected = LockType{ 0 };
    return this->is_locked.compare_exchange_strong(expected, 1);
  }

  auto unlock() -> void { this->is_locked.store(0, std::memory_order_release); }

private:
  std::atomic<LockType> is_locked;
};

#if CXXTRACE_HAVE_OS_SPIN_LOCK
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
class apple_os_spin_lock
{
public:
  auto try_lock() -> bool { return ::OSSpinLockTry(&this->lock); }

  auto unlock() -> void { ::OSSpinLockUnlock(&this->lock); }

private:
  ::OSSpinLock lock{ OS_SPINLOCK_INIT };
};
#pragma clang diagnostic pop
#endif

#if CXXTRACE_HAVE_OS_UNFAIR_LOCK
class apple_os_unfair_lock
{
public:
  auto try_lock() -> bool { return os_unfair_lock_trylock(&this->lock); }

  auto unlock() -> void { os_unfair_lock_unlock(&this->lock); }

private:
  os_unfair_lock lock{ OS_UNFAIR_LOCK_INIT };
};
#endif
}

#endif
