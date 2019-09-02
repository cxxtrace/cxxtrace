#include "thread.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/string.h>
#include <exception>

#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP
#include <pthread.h>
#endif

namespace cxxtrace_test {
auto
set_current_thread_name(cxxtrace::czstring name) -> void
{
#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP
  auto rc = ::pthread_setname_np(name);
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
