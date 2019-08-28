#ifndef CXXTRACE_DETAIL_MUTEX_H
#define CXXTRACE_DETAIL_MUTEX_H

#include <cxxtrace/detail/debug_source_location.h>
#include <mutex>
#include <type_traits>

#if CXXTRACE_ENABLE_RELACY
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winline-new-delete"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <relacy/stdlib/mutex.hpp>

#pragma clang diagnostic pop
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

#if CXXTRACE_ENABLE_CDSCHECKER
class cdschecker_mutex
{
public:
  explicit cdschecker_mutex();

  cdschecker_mutex(const cdschecker_mutex&) = delete;
  cdschecker_mutex& operator=(const cdschecker_mutex&) = delete;

  ~cdschecker_mutex();

  auto lock(debug_source_location) noexcept -> void;
  auto unlock(debug_source_location) noexcept -> void;

private:
  inline auto get() noexcept -> std::mutex&;

  std::aligned_storage_t<16, alignof(void*)> storage_;
};
#endif

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
