#ifndef CXXTRACE_DETAIL_MUTEX_H
#define CXXTRACE_DETAIL_MUTEX_H

#include <cassert>
#include <cxxtrace/detail/debug_source_location.h>
#include <mutex>

#if CXXTRACE_ENABLE_CDSCHECKER
#include <cxxtrace/detail/cdschecker_mutex.h>
#endif

#if CXXTRACE_ENABLE_RELACY
#include <cxxtrace/detail/warning.h>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wextra-semi")
CXXTRACE_WARNING_IGNORE_CLANG("-Winline-new-delete")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wmissing-field-initializers")

#include <relacy/stdlib/mutex.hpp>

CXXTRACE_WARNING_POP
#endif

namespace cxxtrace {
namespace detail {
class real_mutex
{
public:
  auto lock(debug_source_location) noexcept -> void { this->mutex_.lock(); }

  auto unlock(debug_source_location) noexcept -> void { this->mutex_.unlock(); }

private:
  std::mutex mutex_;
};

#if CXXTRACE_ENABLE_RELACY
using relacy_mutex = rl::mutex;
#endif

#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY
struct try_to_lock_t
{};
inline constexpr try_to_lock_t try_to_lock{};
#else
using try_to_lock_t = std::try_to_lock_t;
using std::try_to_lock;
#endif

template<class Mutex>
class lock_guard
{
public:
  explicit lock_guard(Mutex& mutex, debug_source_location caller) noexcept
    : mutex_{ mutex }
    , caller_{ caller }
  {
    this->mutex_.lock(this->caller_);
  }

  lock_guard(const lock_guard&) = delete;
  lock_guard& operator=(const lock_guard&) = delete;

  ~lock_guard() { this->mutex_.unlock(this->caller_); }

private:
  Mutex& mutex_;
  debug_source_location caller_;
};

template<class Mutex>
class unique_lock
{
public:
  explicit unique_lock(Mutex& mutex, debug_source_location caller)
    : mutex_{ mutex }
    , caller_{ caller }
  {
    this->lock(caller);
  }

  explicit unique_lock(Mutex& mutex,
                       try_to_lock_t,
                       debug_source_location caller) noexcept
    : mutex_{ mutex }
    , caller_{ caller }
  {
    this->owns_lock_ = this->mutex_.try_lock(this->caller_);
  }

  unique_lock(const unique_lock&) = delete;
  unique_lock& operator=(const unique_lock&) = delete;

  ~unique_lock()
  {
    if (this->owns_lock()) {
      this->mutex_.unlock(this->caller_);
    }
  }

  auto owns_lock() const noexcept -> bool { return this->owns_lock_; }

  explicit operator bool() const noexcept { return this->owns_lock(); }

  auto lock(debug_source_location caller) -> void
  {
    assert(!this->owns_lock());
    this->mutex_.lock(caller);
    this->owns_lock_ = true;
  }

  auto unlock(debug_source_location caller) -> void
  {
    assert(this->owns_lock());
    this->mutex_.unlock(caller);
    this->owns_lock_ = false;
  }

private:
  Mutex& mutex_;
  debug_source_location caller_;
  bool owns_lock_{ false };
};

#if CXXTRACE_ENABLE_CDSCHECKER
using mutex = cdschecker_mutex;
#elif CXXTRACE_ENABLE_RELACY
using mutex = relacy_mutex;
#else
using mutex = real_mutex;
#endif
}
}

#endif
