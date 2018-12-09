#include <cxxtrace/clock_extra.h>

namespace cxxtrace {
namespace detail {
template<class StandardClock>
auto
std_clock<StandardClock>::query() -> sample
{
  return StandardClock::now();
}

template<class StandardClock>
auto
std_clock<StandardClock>::make_time_point(const sample& sample) -> time_point
{
  // TODO(strager): Handle overflow (e.g. for MSVC's std::chrono::file_clock).
  return time_point{ sample.time_since_epoch() };
}

template class std_clock<std_high_resolution_clock>;
template class std_clock<std_steady_clock>;
template class std_clock<std_system_clock>;
}
}
