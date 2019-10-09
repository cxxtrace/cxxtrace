#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/real_synchronization.h>

namespace cxxtrace {
namespace detail {
backoff::backoff() = default;

auto
backoff::reset() -> void
{}

auto backoff::yield(debug_source_location) -> void
{
  // TODO(strager): Call ::sched_yield or something.
  // TODO(strager): Delete this function. Make callers use mutexes or condition
  // variables or other synchronization primitives instead.
}
}
}
