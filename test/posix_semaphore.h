#ifndef CXXTRACE_TEST_POSIX_SEMAPHORE_H
#define CXXTRACE_TEST_POSIX_SEMAPHORE_H

#include <cxxtrace/detail/have.h>

#if CXXTRACE_HAVE_POSIX_SEMAPHORE
#include <cxxtrace/detail/debug_source_location.h>
#include <semaphore.h>
#endif

namespace cxxtrace_test {
#if CXXTRACE_HAVE_POSIX_SEMAPHORE
class posix_semaphore
{
public:
  explicit posix_semaphore();
  ~posix_semaphore();

  posix_semaphore(const posix_semaphore&) = delete;
  posix_semaphore& operator=(const posix_semaphore&) = delete;

  posix_semaphore(posix_semaphore&&) = delete;
  posix_semaphore& operator=(posix_semaphore&&) = delete;

  auto signal(cxxtrace::detail::stub_debug_source_location) -> void;
  auto wait(cxxtrace::detail::stub_debug_source_location) -> void;

private:
  ::sem_t semaphore_;
};
#endif
}

#endif
