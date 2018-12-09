#ifndef CXXTRACE_CLOCK_IMPL_H
#define CXXTRACE_CLOCK_IMPL_H

#if !defined(CXXTRACE_CLOCK_H)
#error                                                                         \
  "Include <cxxtrace/clock.h> instead of including <cxxtrace/clock_impl.h> directly."
#endif

namespace cxxtrace {
constexpr auto
clock_traits::is_strictly_increasing_per_thread() const noexcept -> bool
{
  switch (this->monotonicity) {
    case clock_monotonicity::strictly_increasing_per_thread:
      return true;
    case clock_monotonicity::non_decreasing_per_thread:
    case clock_monotonicity::not_monotonic:
      return false;
  }
}

constexpr auto
clock_traits::is_non_decreasing_per_thread() const noexcept -> bool
{
  switch (this->monotonicity) {
    case clock_monotonicity::strictly_increasing_per_thread:
    case clock_monotonicity::non_decreasing_per_thread:
      return true;
    case clock_monotonicity::not_monotonic:
      return false;
  }
}
}

#endif
