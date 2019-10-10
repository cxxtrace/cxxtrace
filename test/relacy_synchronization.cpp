#if !CXXTRACE_ENABLE_RELACY
#error "CXXTRACE_ENABLE_RELACY must be defined and non-zero."
#endif

#include "relacy_backoff.h"
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")
// clang-format off
#include <relacy/thread_local_ctx.hpp> // IWYU pragma: keep
// clang-format on
#include <relacy/backoff.hpp>
CXXTRACE_WARNING_POP

namespace cxxtrace_test {
relacy_backoff::relacy_backoff() = default;

relacy_backoff::~relacy_backoff() = default;

auto
relacy_backoff::reset() -> void
{
  this->backoff = rl::linear_backoff{};
}

auto
relacy_backoff::yield(cxxtrace::detail::debug_source_location caller) -> void
{
  this->backoff.yield(caller);
}
}
