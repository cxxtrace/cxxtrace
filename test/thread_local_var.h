#ifndef CXXTRACE_TEST_THREAD_LOCAL_VAR_H
#define CXXTRACE_TEST_THREAD_LOCAL_VAR_H

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>
#include <exception>
#include <pthread.h>
#include <system_error>
#include <type_traits>

#if CXXTRACE_ENABLE_CDSCHECKER
#include "cdschecker_thread.h"
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include "cxxtrace_concurrency_test.h"
#include <memory>
#endif

#if CXXTRACE_ENABLE_RELACY
CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
#include <relacy/thread_local.hpp>
CXXTRACE_WARNING_POP
#endif

namespace cxxtrace_test {
static constexpr auto thread_local_var_maximum_thread_count = 3;

#if CXXTRACE_ENABLE_CDSCHECKER
template<class T>
class cdschecker_thread_local_var
{
public:
  static_assert(std::is_trivial_v<T>);

  auto operator-> () noexcept -> T* { return this->get(); }

  auto operator*() noexcept -> T& { return *this->get(); }

  auto get() noexcept -> T*
  {
    auto current_thread_id = cdschecker_current_thread_id();
    assert(current_thread_id >= this->first_thread_id);
    auto slot_index = current_thread_id - this->first_thread_id;
    // If this assertion fails, increase thread_local_var_maximum_thread_count.
    assert(std::size_t(slot_index) < this->slots_.size());
    auto& slot = this->slots_[slot_index];

    if (!slot.allocated) {
      // slot.thread_id = current_thread_id;
      slot.allocated = true;
      std::memset(&slot.value, 0, sizeof(slot.value));
    }
    return &slot.value;
  }

private:
  struct slot
  {
    T value /* uninitialized */;
    bool allocated{ false };
  };

  // Thread ID 0 is unused.
  // Thread ID 1 is used by the main thread which spawns each test thread. See
  // run_concurrency_test_from_cdschecker.
  static constexpr auto first_thread_id = cdschecker_thread_id{ 2 };

  std::array<slot, thread_local_var_maximum_thread_count> slots_;
};
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
template<class T>
class pthread_thread_local_var
{
public:
  static_assert(std::is_trivial_v<T>);

  explicit pthread_thread_local_var() noexcept(false)
    : thread_state_{ pthread_key_create_checked(deallocate_thread_state) }
  {}

  pthread_thread_local_var(const pthread_thread_local_var&) = delete;
  pthread_thread_local_var& operator=(const pthread_thread_local_var&) = delete;

  ~pthread_thread_local_var()
  {
    auto rc = ::pthread_key_delete(this->thread_state_);
    if (rc != 0) {
      std::perror(
        "fatal: failed to set pthread_thread_local_var data thread-local key");
      std::terminate();
    }
  }

  auto operator-> () noexcept -> T* { return this->get(); }

  auto operator*() noexcept -> T& { return *this->get(); }

  auto get() noexcept -> T*
  {
    auto* current_thread_state =
      static_cast<thread_state*>(::pthread_getspecific(this->thread_state_));
    auto current_generation = current_concurrency_stress_generation();
    if (current_thread_state) {
      if (current_thread_state->initialized_generation != current_generation) {
        current_thread_state->reset(current_generation);
      }
    } else {
      auto unique_thread_state =
        std::make_unique<thread_state>(current_generation);
      current_thread_state = unique_thread_state.get();
      auto rc =
        ::pthread_setspecific(this->thread_state_, current_thread_state);
      if (rc != 0) {
        std::perror("fatal: failed to set pthread_thread_local_var data for "
                    "current thread");
        std::terminate();
      }
      unique_thread_state.release();
    }
    return &current_thread_state->value;
  }

private:
  struct thread_state
  {
    explicit thread_state(
      concurrency_stress_generation current_generation) noexcept
    {
      this->reset(current_generation);
    }

    auto reset(concurrency_stress_generation current_generation) noexcept
      -> void
    {
      this->initialized_generation = current_generation;
      std::memset(&this->value, 0, sizeof(this->value));
    }

    concurrency_stress_generation initialized_generation /* uninitialized */;
    T value /* uninitialized */;
  };

  static auto pthread_key_create_checked(void (*destructor)(void*)) noexcept(
    false) -> ::pthread_key_t
  {
    ::pthread_key_t key;
    auto rc = ::pthread_key_create(&key, destructor);
    if (rc != 0) {
      throw std::system_error{ rc, std::generic_category() };
    }
    return key;
  }

  static auto deallocate_thread_state(void* opaque_thread_state) noexcept
    -> void
  {
    deallocate_thread_state(static_cast<thread_state*>(opaque_thread_state));
  }

  static auto deallocate_thread_state(thread_state* state) noexcept -> void
  {
    auto unique_thread_state = std::unique_ptr<thread_state>(state);
    unique_thread_state.reset();
  }

  ::pthread_key_t thread_state_;
};
#endif

#if CXXTRACE_ENABLE_RELACY
template<class T>
class relacy_thread_local_var
{
public:
  static_assert(std::is_trivial_v<T>);

  auto operator-> () noexcept -> T* { return this->get(); }

  auto operator*() noexcept -> T& { return *this->get(); }

  auto get() noexcept -> T*
  {
    auto thread_index = rl::thread_index();
    // If this assertion fails, increase thread_local_var_maximum_thread_count.
    assert(thread_index < slots_.size());
    auto& slot = this->slots_[thread_index];
    if (!this->slot_initialized_.get(CXXTRACE_HERE)) {
      std::memset(&slot, 0, sizeof(slot));
      this->slot_initialized_.set(true, CXXTRACE_HERE);
    }
    return &slot;
  }

private:
  rl::thread_local_var<bool> slot_initialized_{};
  std::array<T, thread_local_var_maximum_thread_count> slots_;
};
#endif

#if CXXTRACE_ENABLE_CDSCHECKER
template<class T>
using thread_local_var = cdschecker_thread_local_var<T>;
#elif CXXTRACE_ENABLE_CONCURRENCY_STRESS
template<class T>
using thread_local_var = pthread_thread_local_var<T>;
#elif CXXTRACE_ENABLE_RELACY
template<class T>
using thread_local_var = relacy_thread_local_var<T>;
#else
#error "Unknown platform"
#endif
}

#endif
