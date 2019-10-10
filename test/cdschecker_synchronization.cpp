#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_backoff.h"
#include <cxxtrace/detail/cdschecker.h>
#include <cxxtrace/detail/debug_source_location.h>

namespace cxxtrace_test {
cdschecker_backoff::cdschecker_backoff() = default;

cdschecker_backoff::~cdschecker_backoff() = default;

auto
cdschecker_backoff::reset() -> void
{}

auto cdschecker_backoff::yield(cxxtrace::detail::debug_source_location) -> void
{
  cxxtrace::detail::cdschecker::thrd_yield();
}
}
