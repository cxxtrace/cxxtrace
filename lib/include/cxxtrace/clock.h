#ifndef CXXTRACE_CLOCK_H
#define CXXTRACE_CLOCK_H

#include <chrono>
#include <cstdint>
#include <cxxtrace/detail/have.h>
#include <iosfwd>
#include <sys/time.h>

#if CXXTRACE_HAVE_MACH_TIME
#include <mach/mach_time.h>
#endif

#if CXXTRACE_HAVE_CLOCK_GETTIME
#include <time.h>
#endif

namespace cxxtrace {
struct uninitialized_t;

class time_point
{
public:
  explicit time_point(uninitialized_t) noexcept;
  explicit time_point(std::chrono::nanoseconds time_since_reference) noexcept;

  std::chrono::nanoseconds nanoseconds_since_reference() const;

private:
  friend auto operator!=(const time_point&, const time_point&) -> bool;
  friend auto operator<(const time_point&, const time_point&) -> bool;
  friend auto operator<=(const time_point&, const time_point&) -> bool;
  friend auto operator==(const time_point&, const time_point&) -> bool;
  friend auto operator>(const time_point&, const time_point&) -> bool;
  friend auto operator>=(const time_point&, const time_point&) -> bool;

  friend auto operator<<(std::ostream&, const time_point&) -> std::ostream&;

  std::chrono::nanoseconds time_since_reference;
};

class clock_base
{};

enum class clock_monotonicity
{
  not_monotonic,
  strictly_increasing_per_thread,
  non_decreasing_per_thread,
};

struct clock_traits
{
  constexpr auto is_strictly_increasing_per_thread() const noexcept -> bool;

  constexpr auto is_non_decreasing_per_thread() const noexcept -> bool;

  clock_monotonicity monotonicity;
};

#if CXXTRACE_HAVE_MACH_TIME
namespace detail {
inline constexpr auto
apple_absolute_time_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::strictly_increasing_per_thread,
  };
}
}

class apple_absolute_time_clock : public clock_base
{
public:
  using sample = std::uint64_t;

  inline static constexpr auto traits =
    detail::apple_absolute_time_clock_traits();

  explicit apple_absolute_time_clock() noexcept(false);

  auto query() -> sample;

  auto make_time_point(const sample&) -> time_point;

private:
  ::mach_timebase_info_data_t time_base;
};

namespace detail {
inline constexpr auto
apple_approximate_time_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::non_decreasing_per_thread,
  };
}
}

class apple_approximate_time_clock : public clock_base
{
public:
  using sample = apple_absolute_time_clock::sample;

  inline static constexpr auto traits =
    detail::apple_approximate_time_clock_traits();

  explicit apple_approximate_time_clock() noexcept(false);

  auto query() -> sample;

  auto make_time_point(const sample&) -> time_point;

private:
  apple_absolute_time_clock absolute_time_clock_;
};
#endif

#if CXXTRACE_HAVE_CLOCK_GETTIME
namespace detail {
template<::clockid_t ClockID>
inline constexpr auto
posix_clock_gettime_clock() noexcept -> clock_traits = delete;

template<>
inline constexpr auto
posix_clock_gettime_clock<CLOCK_MONOTONIC>() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::non_decreasing_per_thread,
  };
}

struct posix_clock_gettime_clock_sample
{
  ::timespec time;

  friend auto operator==(posix_clock_gettime_clock_sample,
                         posix_clock_gettime_clock_sample) noexcept -> bool;
  friend auto operator!=(posix_clock_gettime_clock_sample,
                         posix_clock_gettime_clock_sample) noexcept -> bool;
};
}

template<::clockid_t ClockID>
class posix_clock_gettime_clock;

template<::clockid_t ClockID>
class posix_clock_gettime_clock : public clock_base
{
public:
  using sample = detail::posix_clock_gettime_clock_sample;

  inline static constexpr auto traits =
    detail::posix_clock_gettime_clock<ClockID>();

  auto query() -> sample;

  auto make_time_point(const sample&) -> time_point;
};

extern template class posix_clock_gettime_clock<CLOCK_MONOTONIC>;
#endif

namespace detail {
inline constexpr auto
posix_gettimeofday_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::not_monotonic,
  };
}
}

class posix_gettimeofday_clock : public clock_base
{
public:
  struct sample
  {
    ::timeval time;

    friend auto operator==(sample, sample) noexcept -> bool;
    friend auto operator!=(sample, sample) noexcept -> bool;
  };

  inline static constexpr auto traits =
    detail::posix_gettimeofday_clock_traits();

  auto query() -> sample;

  auto make_time_point(const sample&) -> time_point;
};

namespace detail {
inline constexpr auto
fake_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::strictly_increasing_per_thread,
  };
}
}

class fake_clock : public clock_base
{
public:
  using sample = std::chrono::nanoseconds::rep;

  inline static constexpr auto traits = detail::fake_clock_traits();

  explicit fake_clock() noexcept;

  fake_clock(const fake_clock&) = delete;
  fake_clock& operator=(const fake_clock&) = delete;

  auto query() -> sample;
  auto make_time_point(const sample&) -> time_point;

  auto set_duration_between_samples(std::chrono::nanoseconds) noexcept -> void;
  auto set_next_time_point(std::chrono::nanoseconds) noexcept -> void;

private:
  sample next_sample;
  sample query_increment;
};

using default_clock =
#if CXXTRACE_HAVE_MACH_TIME
  apple_absolute_time_clock
#else
  posix_gettimeofday_clock
#endif
  ;
}

#include <cxxtrace/clock_impl.h> // IWYU pragma: export

#endif
