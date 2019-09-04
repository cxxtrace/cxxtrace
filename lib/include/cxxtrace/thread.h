#ifndef CXXTRACE_THREAD_H
#define CXXTRACE_THREAD_H

#include <cxxtrace/detail/have.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <cstdint>
#endif

#if defined(__linux__)
#include <sys/types.h>
#endif

namespace cxxtrace {
using thread_id =
#if defined(__APPLE__) && defined(__MACH__)
  std::uint64_t
#elif defined(__linux__)
  ::pid_t
#else
#error "Unsupported platform"
#endif
  ;

auto
get_current_thread_id() noexcept -> thread_id;

#if CXXTRACE_HAVE_MACH_THREAD
auto
get_current_thread_mach_thread_id() noexcept -> std::uint64_t;
#endif

#if CXXTRACE_HAVE_PTHREAD_THREADID_NP
auto
get_current_thread_pthread_thread_id() noexcept -> std::uint64_t;
#endif

#if CXXTRACE_HAVE_SYSCALL_GETTID
auto
get_current_thread_id_gettid() noexcept -> thread_id;
#endif
}

#endif
