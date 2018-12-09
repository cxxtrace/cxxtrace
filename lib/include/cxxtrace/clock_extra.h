#ifndef CXXTRACE_CLOCK_EXTRA_H
#define CXXTRACE_CLOCK_EXTRA_H

#include <chrono>
#include <cxxtrace/clock.h>

namespace cxxtrace {
namespace detail {
template<class StandardClock>
constexpr auto
std_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = StandardClock::is_steady
                      ? clock_monotonicity::non_decreasing_per_thread
                      : clock_monotonicity::not_monotonic,
  };
}

template<class StandardClock>
class std_clock : public clock_base
{
public:
  using sample = typename StandardClock::time_point;

  inline static constexpr auto traits = std_clock_traits<StandardClock>();

  auto query() -> sample;

  auto make_time_point(const sample&) -> time_point;
};

class std_high_resolution_clock : public std::chrono::high_resolution_clock
{};
extern template class std_clock<std::chrono::high_resolution_clock>;

class std_steady_clock : public std::chrono::steady_clock
{};
extern template class std_clock<std::chrono::steady_clock>;

class std_system_clock : public std::chrono::system_clock
{};
extern template class std_clock<std::chrono::system_clock>;
}

class std_high_resolution_clock
  : public detail::std_clock<detail::std_high_resolution_clock>
{};

class std_steady_clock : public detail::std_clock<detail::std_steady_clock>
{};

class std_system_clock : public detail::std_clock<detail::std_system_clock>
{};
}

#endif
