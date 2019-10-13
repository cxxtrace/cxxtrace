#include <cxxtrace/detail/have.h> // IWYU pragma: keep

#if CXXTRACE_HAVE_LIBDISPATCH_SEMAPHORE
#include "libdispatch_semaphore.h"
#include <cxxtrace/detail/debug_source_location.h>
#include <dispatch/dispatch.h>
#endif

namespace cxxtrace_test {
#if CXXTRACE_HAVE_LIBDISPATCH_SEMAPHORE
libdispatch_semaphore::libdispatch_semaphore()
  : semaphore_{ ::dispatch_semaphore_create(0) }
{}

libdispatch_semaphore::~libdispatch_semaphore()
{
  ::dispatch_release(this->semaphore_);
}

auto libdispatch_semaphore::signal(cxxtrace::detail::stub_debug_source_location)
  -> void
{
  ::dispatch_semaphore_signal(this->semaphore_);
}

auto libdispatch_semaphore::wait(cxxtrace::detail::stub_debug_source_location)
  -> void
{
  ::dispatch_semaphore_wait(this->semaphore_, DISPATCH_TIME_FOREVER);
}
#endif
}
