#ifndef CXXTRACE_TEST_LIBDISPATCH_SEMAPHORE_H
#define CXXTRACE_TEST_LIBDISPATCH_SEMAPHORE_H

#include <cxxtrace/detail/have.h>

#if CXXTRACE_HAVE_LIBDISPATCH_SEMAPHORE
#include <cxxtrace/detail/debug_source_location.h>
#include <dispatch/dispatch.h>
#endif

namespace cxxtrace_test {
#if CXXTRACE_HAVE_LIBDISPATCH_SEMAPHORE
class libdispatch_semaphore
{
public:
  explicit libdispatch_semaphore();
  ~libdispatch_semaphore();

  libdispatch_semaphore(const libdispatch_semaphore&) = delete;
  libdispatch_semaphore& operator=(const libdispatch_semaphore&) = delete;

  libdispatch_semaphore(libdispatch_semaphore&&) = delete;
  libdispatch_semaphore& operator=(libdispatch_semaphore&&) = delete;

  auto signal(cxxtrace::detail::stub_debug_source_location) -> void;
  auto wait(cxxtrace::detail::stub_debug_source_location) -> void;

private:
  ::dispatch_semaphore_t semaphore_;
};
#endif
}

#endif
