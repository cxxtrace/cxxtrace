#include <cxxtrace/detail/have.h> // IWYU pragma: keep

#if CXXTRACE_HAVE_POSIX_SEMAPHORE
#include "posix_semaphore.h"
#include <cstdio>
#include <exception>
#include <semaphore.h>
#endif

namespace cxxtrace_test {
#if CXXTRACE_HAVE_POSIX_SEMAPHORE
posix_semaphore::posix_semaphore()
{
  auto pshared = false;
  auto rc = ::sem_init(&this->semaphore_, pshared, 0);
  if (rc != 0) {
    std::perror("error: posix_semaphore sem_init failed");
    std::terminate();
  }
}

posix_semaphore::~posix_semaphore()
{
  auto rc = ::sem_destroy(&this->semaphore_);
  if (rc != 0) {
    std::perror("error: posix_semaphore sem_destroy failed");
    std::terminate();
  }
}

auto posix_semaphore::signal(cxxtrace::detail::stub_debug_source_location)
  -> void
{
  auto rc = ::sem_post(&this->semaphore_);
  if (rc != 0) {
    std::perror("error: posix_semaphore sem_post failed");
    std::terminate();
  }
}

auto posix_semaphore::wait(cxxtrace::detail::stub_debug_source_location) -> void
{
  auto rc = ::sem_wait(&this->semaphore_);
  if (rc != 0) {
    std::perror("error: posix_semaphore sem_wait failed");
    std::terminate();
  }
}
#endif
}
