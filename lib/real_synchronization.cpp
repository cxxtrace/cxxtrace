#include <cxxtrace/detail/real_synchronization.h>

namespace cxxtrace {
namespace detail {
real_synchronization::backoff::backoff() = default;

auto
real_synchronization::backoff::reset() -> void
{}

auto real_synchronization::backoff::yield(debug_source_location) -> void
{
  // TODO(strager): Call ::sched_yield or something.
  // TODO(strager): Delete this function. Make callers use mutexes or condition
  // variables or other synchronization primitives instead.
}
}
}
