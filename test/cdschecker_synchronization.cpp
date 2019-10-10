#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_synchronization.h"

namespace cxxtrace_test {
cdschecker_synchronization::backoff::backoff() = default;

cdschecker_synchronization::backoff::~backoff() = default;

auto
cdschecker_synchronization::backoff::reset() -> void
{}

auto cdschecker_synchronization::backoff::yield(debug_source_location) -> void
{
  cxxtrace::detail::cdschecker::thrd_yield();
}
}
