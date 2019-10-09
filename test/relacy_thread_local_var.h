#ifndef CXXTRACE_TEST_RELACY_THREAD_LOCAL_VAR_H
#define CXXTRACE_TEST_RELACY_THREAD_LOCAL_VAR_H

#if CXXTRACE_ENABLE_RELACY
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

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wextra-semi")
CXXTRACE_WARNING_IGNORE_CLANG("-Winline-new-delete")
CXXTRACE_WARNING_IGNORE_GCC("-Wsized-deallocation")
// clang-format off
#include <relacy/thread_local_ctx.hpp>
// clang-format on
#include <relacy/thread_local.hpp>
CXXTRACE_WARNING_POP
#endif

namespace cxxtrace_test {
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
#endif
}

#endif
