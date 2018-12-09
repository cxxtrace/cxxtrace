#include <cassert>
#include <chrono>
#include <cxxtrace/clock.h>
#include <cxxtrace/uninitialized.h>
#include <stdexcept>
#include <utility>

#if defined(__APPLE__)
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#endif

namespace cxxtrace {
time_point::time_point(uninitialized_t) noexcept
  : time_point{ std::chrono::nanoseconds::zero() }
{}

time_point::time_point(std::chrono::nanoseconds time_since_reference) noexcept
  : time_since_reference{ time_since_reference }
{}

std::chrono::nanoseconds
time_point::nanoseconds_since_reference() const
{
  return this->time_since_reference;
}

auto
operator!=(const time_point& left, const time_point& right) -> bool
{
  return !(left == right);
}

auto
operator==(const time_point& left, const time_point& right) -> bool
{
  return left.time_since_reference == right.time_since_reference;
}

auto
operator<(const time_point& left, const time_point& right) -> bool
{
  return left.time_since_reference < right.time_since_reference;
}

auto
operator<=(const time_point& left, const time_point& right) -> bool
{
  return left.time_since_reference <= right.time_since_reference;
}

auto
operator>(const time_point& left, const time_point& right) -> bool
{
  return left.time_since_reference > right.time_since_reference;
}

auto
operator>=(const time_point& left, const time_point& right) -> bool
{
  return left.time_since_reference >= right.time_since_reference;
}

#if defined(__APPLE__)
apple_absolute_time_clock::apple_absolute_time_clock() noexcept(false)
{
  auto rc = ::mach_timebase_info(&this->time_base);
  if (rc != KERN_SUCCESS) {
    ::mach_error("error: could not calibrate clock:", rc);
    throw std::runtime_error("mach_timebase_info failed");
  }
  assert(this->time_base.numer != 0);
  assert(this->time_base.denom != 0);
}

auto
apple_absolute_time_clock::query() -> sample
{
  return sample{ ::mach_absolute_time() };
}

auto
apple_absolute_time_clock::make_time_point(const sample& sample) -> time_point
{
  if (this->time_base.numer != this->time_base.denom) {
    // TODO(strager): Support different time bases.
    throw std::runtime_error{ "Apple time bases not yet implemented" };
  }
  return time_point{ std::chrono::nanoseconds{ sample.absolute_time } };
}
#endif

fake_clock::fake_clock() noexcept
  : next_sample{ std::chrono::nanoseconds{ 1 }.count() }
{}

auto
fake_clock::query() -> sample
{
  return std::exchange(this->next_sample,
                       (std::chrono::nanoseconds{ this->next_sample } +
                        std::chrono::nanoseconds{ 1 })
                         .count());
}

auto
fake_clock::make_time_point(const sample& sample) -> time_point
{
  return time_point{ std::chrono::nanoseconds{ sample } };
}
}
