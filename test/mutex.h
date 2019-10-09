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
#include <cxxtrace/detail/warning.h>
#include <libkern/OSSpinLockDeprecated.h>
#endif

#if CXXTRACE_HAVE_OS_UNFAIR_LOCK
#include <os/lock.h>
#endif

namespace cxxtrace_test {
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
CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated")
class apple_os_spin_lock
{
public:
  auto try_lock() -> bool { return ::OSSpinLockTry(&this->lock); }

  auto unlock() -> void { ::OSSpinLockUnlock(&this->lock); }

private:
  ::OSSpinLock lock{ OS_SPINLOCK_INIT };
};
CXXTRACE_WARNING_POP
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
