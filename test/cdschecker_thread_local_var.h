#ifndef CXXTRACE_TEST_CDSCHECKER_THREAD_LOCAL_VAR_H
#define CXXTRACE_TEST_CDSCHECKER_THREAD_LOCAL_VAR_H

#if CXXTRACE_ENABLE_CDSCHECKER
#include "cdschecker_thread.h"
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
#endif

namespace cxxtrace_test {
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
    // If this assertion fails, increase maximum_thread_count.
    assert(std::size_t(slot_index) < this->slots_.size());
    auto& slot = this->slots_[slot_index];

    if (!slot.allocated) {
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

  static constexpr auto maximum_thread_count = 3;

  std::array<slot, maximum_thread_count> slots_;
};
#endif
}

#endif
