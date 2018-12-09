#ifndef CXXTRACE_CLOCK_H
#define CXXTRACE_CLOCK_H

#include <chrono>
#include <cstdint>
#include <cxxtrace/uninitialized.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

namespace cxxtrace {
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

  std::chrono::nanoseconds time_since_reference;
};

class clock_base
{};

enum class clock_reference
{
  arbitrary,
};

enum class clock_monotonicity
{
  strictly_increasing_per_thread,
};

struct clock_traits
{
  constexpr auto is_strictly_increasing_per_thread() const noexcept -> bool
  {
    switch (this->monotonicity) {
      case clock_monotonicity::strictly_increasing_per_thread:
        return true;
    }
  }

  clock_monotonicity monotonicity;
  clock_reference reference;
};

namespace detail {
inline constexpr auto
apple_absolute_time_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::strictly_increasing_per_thread,
    .reference = clock_reference::arbitrary,
  };
}
}

class apple_absolute_time_clock : public clock_base
{
public:
  struct sample
  {
    std::uint64_t absolute_time;
  };

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
fake_clock_traits() noexcept -> clock_traits
{
  return clock_traits{
    .monotonicity = clock_monotonicity::strictly_increasing_per_thread,
    .reference = clock_reference::arbitrary,
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

private:
  sample next_sample;
};
}

#endif
