#include <cassert>
#include <chrono>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/uninitialized.h>
#include <ostream>
#include <stdexcept>
#include <sys/time.h>
#include <utility>
// IWYU pragma: no_include <ratio>

#if CXXTRACE_HAVE_MACH_TIME
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#endif

#if CXXTRACE_HAVE_CLOCK_GETTIME
#include <time.h>
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
operator<<(std::ostream& out, const time_point& time) -> std::ostream&
{
  out << '+' << time.nanoseconds_since_reference().count() << "ns";
  return out;
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

#if CXXTRACE_HAVE_MACH_TIME
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
  return time_point{ std::chrono::nanoseconds{ sample } };
}

apple_approximate_time_clock::apple_approximate_time_clock() noexcept(false) =
  default;

auto
apple_approximate_time_clock::query() -> sample
{
  return sample{ ::mach_approximate_time() };
}

auto
apple_approximate_time_clock::make_time_point(const sample& sample)
  -> time_point
{
  return this->absolute_time_clock_.make_time_point(sample);
}
#endif

auto
operator==(posix_gettimeofday_clock::sample x,
           posix_gettimeofday_clock::sample y) noexcept -> bool
{
  return x.time.tv_sec == y.time.tv_sec && x.time.tv_usec == y.time.tv_usec;
}

auto
operator!=(posix_gettimeofday_clock::sample x,
           posix_gettimeofday_clock::sample y) noexcept -> bool
{
  return !(x == y);
}

#if CXXTRACE_HAVE_CLOCK_GETTIME
namespace detail {
auto
operator==(posix_clock_gettime_clock_sample x,
           posix_clock_gettime_clock_sample y) noexcept -> bool
{
  return x.time.tv_sec == y.time.tv_sec && x.time.tv_nsec == y.time.tv_nsec;
}

auto
operator!=(posix_clock_gettime_clock_sample x,
           posix_clock_gettime_clock_sample y) noexcept -> bool
{
  return !(x == y);
}
}

template<::clockid_t ClockID>
auto
posix_clock_gettime_clock<ClockID>::query() -> sample
{
  auto now = sample{};
  [[maybe_unused]] auto rc = ::clock_gettime(ClockID, &now.time);
  // TODO(strager): In posix_clock_gettime_clock's constructor, check if the
  // clock is supported.
  assert(rc == 0);
  return now;
}

template<::clockid_t ClockID>
auto
posix_clock_gettime_clock<ClockID>::make_time_point(const sample& sample)
  -> time_point
{
  // TODO(strager): Handle overflow.
  return time_point{ std::chrono::seconds{ sample.time.tv_sec } +
                     std::chrono::nanoseconds{ sample.time.tv_nsec } };
}

template class posix_clock_gettime_clock<CLOCK_MONOTONIC>;
#endif

auto
posix_gettimeofday_clock::query() -> sample
{
  auto now = sample{};
  [[maybe_unused]] auto rc = ::gettimeofday(&now.time, nullptr);
  assert(rc == 0);
  return now;
}

auto
posix_gettimeofday_clock::make_time_point(const sample& sample) -> time_point
{
  // TODO(strager): Handle overflow.
  return time_point{ std::chrono::seconds{ sample.time.tv_sec } +
                     std::chrono::microseconds{ sample.time.tv_usec } };
}

fake_clock::fake_clock() noexcept
  : next_sample{ std::chrono::nanoseconds{ 1 }.count() }
  , query_increment{ std::chrono::nanoseconds{ 1 }.count() }
{}

auto
fake_clock::query() -> sample
{
  return std::exchange(this->next_sample,
                       (std::chrono::nanoseconds{ this->next_sample } +
                        std::chrono::nanoseconds{ this->query_increment })
                         .count());
}

auto
fake_clock::make_time_point(const sample& sample) -> time_point
{
  return time_point{ std::chrono::nanoseconds{ sample } };
}

auto
fake_clock::set_duration_between_samples(
  std::chrono::nanoseconds duration) noexcept -> void
{
  this->query_increment = duration.count();
}

auto
fake_clock::set_next_time_point(
  std::chrono::nanoseconds next_time_point) noexcept -> void
{
  auto new_next_sample = sample{ next_time_point.count() };
  assert(new_next_sample >= this->next_sample &&
         "fake_clock is strictly increasing");
  this->next_sample = new_next_sample;
}
}
