#include "thread.h"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/string.h>
#include <exception>
#include <thread>

#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP
#include <pthread.h>
#endif

using namespace std::literals::chrono_literals;

namespace cxxtrace_test {
auto
await_thread_exit([[maybe_unused]] std::thread& t) -> void
{
  assert(!t.joinable());

#if defined(__APPLE__)
  // HACK(strager): Work around flakiness caused by std::thread::join not
  // waiting for kernel resources to be released (with Clang 8 on macOS 10.12).
  std::this_thread::sleep_for(10ms);
#endif
}

auto
set_current_thread_name(cxxtrace::czstring name) -> void
{
#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP
  auto rc =
#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP_1
    ::pthread_setname_np(name)
#elif CXXTRACE_HAVE_PTHREAD_SETNAME_NP_2
    ::pthread_setname_np(::pthread_self(), name)
#else
#error "Unknown platform"
#endif
    ;
  if (rc != 0) {
    std::fprintf(
      stderr, "fatal: pthread_setname_np failed: %s\n", std::strerror(errno));
    std::terminate();
  }
#else
#error "Unknown platform"
#endif
}
}
