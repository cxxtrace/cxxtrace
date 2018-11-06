#ifndef CXXTRACE_THREAD_H
#define CXXTRACE_THREAD_H

#if defined(__APPLE__) && defined(__MACH__)
#include <cstdint>
#endif

namespace cxxtrace {
using thread_id =
#if defined(__APPLE__) && defined(__MACH__)
  std::uint64_t
#else
#error "Unsupported platform"
#endif
  ;

auto
get_current_thread_id() noexcept -> thread_id;

#if defined(__APPLE__) && defined(__MACH__)
auto
get_current_thread_mach_thread_id() noexcept -> std::uint64_t;

auto
get_current_thread_pthread_thread_id() noexcept -> std::uint64_t;
#endif
}

#endif
