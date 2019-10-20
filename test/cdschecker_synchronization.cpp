#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_synchronization.h"
#include <cxxtrace/detail/workarounds.h>

namespace cxxtrace_test {
cdschecker_synchronization::backoff::backoff() = default;

cdschecker_synchronization::backoff::~backoff() = default;

auto cdschecker_synchronization::allow_preempt(debug_source_location) -> void
{
#if CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
  // Do nothing.
#else
#error "allow_preempt not implemented"
#endif
}

auto
cdschecker_synchronization::backoff::reset() -> void
{}

auto cdschecker_synchronization::backoff::yield(debug_source_location) -> void
{
  cxxtrace::detail::cdschecker::thrd_yield();
}
}
