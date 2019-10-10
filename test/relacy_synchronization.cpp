#if !CXXTRACE_ENABLE_RELACY
#error "CXXTRACE_ENABLE_RELACY must be defined and non-zero."
#endif

#include "relacy_synchronization.h"

namespace cxxtrace_test {
relacy_synchronization::backoff::backoff() = default;

relacy_synchronization::backoff::~backoff() = default;

auto
relacy_synchronization::backoff::reset() -> void
{
  this->backoff_ = rl::linear_backoff{};
}

auto
relacy_synchronization::backoff::yield(debug_source_location caller) -> void
{
  this->backoff_.yield(caller);
}
}
