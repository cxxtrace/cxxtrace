#include "cxxtrace_algorithm.h" // IWYU pragma: keep
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cxxtrace/clock.h>
#include <cxxtrace/clock_extra.h> // IWYU pragma: keep
#include <cxxtrace/detail/have.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iterator>
#include <ratio>
#include <thread>
#include <type_traits> // IWYU pragma: keep
#include <utility>
#include <vector>
// IWYU pragma: no_include "cxxtrace_algorithm_impl.h"

#if CXXTRACE_HAVE_CLOCK_GETTIME
#include <time.h>
#endif

using namespace std::chrono_literals;
using testing::ElementsAre;

namespace cxxtrace_test {
namespace {
template<class Clock>
auto
sample_clock_n(Clock&, int sample_count) noexcept(false)
  -> std::vector<typename Clock::sample>;
}

template<class Clock>
class test_clock : public testing::Test
{
public:
  using clock_type = Clock;
};

using test_clock_types = ::testing::Types<
#if CXXTRACE_HAVE_MACH_TIME
  cxxtrace::apple_absolute_time_clock,
  cxxtrace::apple_approximate_time_clock,
#endif
  cxxtrace::fake_clock,
#if CXXTRACE_HAVE_CLOCK_GETTIME
  cxxtrace::posix_clock_gettime_clock<CLOCK_MONOTONIC>,
#endif
  cxxtrace::posix_gettimeofday_clock,
  cxxtrace::std_high_resolution_clock,
  cxxtrace::std_steady_clock,
  cxxtrace::std_system_clock>;
TYPED_TEST_CASE(test_clock, test_clock_types, );

TYPED_TEST(test_clock, clock_samples_are_comparable)
{
  using clock_type = typename TestFixture::clock_type;
  using sample_type = typename clock_type::sample;

  static_assert(std::is_same_v<decltype(std::declval<sample_type>() ==
                                        std::declval<sample_type>()),
                               bool>);
  static_assert(std::is_same_v<decltype(std::declval<sample_type>() !=
                                        std::declval<sample_type>()),
                               bool>);
}

template<class Clock>
class test_strictly_increasing_clock : public testing::Test
{
public:
  using clock_type = Clock;
};

using test_strictly_increasing_clock_types = ::testing::Types<
#if CXXTRACE_HAVE_MACH_TIME
  cxxtrace::apple_absolute_time_clock,
#endif
  cxxtrace::fake_clock>;
TYPED_TEST_CASE(test_strictly_increasing_clock,
                test_strictly_increasing_clock_types, );

TYPED_TEST(test_strictly_increasing_clock, clock_is_strictly_increasing)
{
  using clock_type = typename TestFixture::clock_type;
  using sample_type = typename clock_type::sample;

  static_assert(clock_type::traits.is_strictly_increasing_per_thread());

  auto sample_count = 64 * 1024;
  auto iteration_count = 10;

  auto clock = clock_type{};

  for (auto iteration = 0; iteration < iteration_count; ++iteration) {
    auto samples = sample_clock_n(clock, sample_count);
    auto non_increasing_it = std::adjacent_find(
      samples.begin(),
      samples.end(),
      [&clock](const sample_type& x, const sample_type& y) noexcept->bool {
        return !(clock.make_time_point(x) < clock.make_time_point(y));
      });
    EXPECT_EQ(non_increasing_it, samples.end());
  }
}

template<class Clock>
class test_non_decreasing_clock : public testing::Test
{
public:
  using clock_type = Clock;
};

using test_non_decreasing_clock_types = ::testing::Types<
#if CXXTRACE_HAVE_MACH_TIME
  cxxtrace::apple_absolute_time_clock,
  cxxtrace::apple_approximate_time_clock,
#endif
  cxxtrace::fake_clock,
#if CXXTRACE_HAVE_CLOCK_GETTIME
  cxxtrace::posix_clock_gettime_clock<CLOCK_MONOTONIC>,
#endif
  cxxtrace::std_steady_clock>;
TYPED_TEST_CASE(test_non_decreasing_clock, test_non_decreasing_clock_types, );

TYPED_TEST(test_non_decreasing_clock, clock_is_non_decreasing)
{
  using clock_type = typename TestFixture::clock_type;
  using sample_type = typename clock_type::sample;

  static_assert(clock_type::traits.is_non_decreasing_per_thread());

  auto sample_count = 64 * 1024;
  auto iteration_count = 10;

  auto clock = clock_type{};

  for (auto iteration = 0; iteration < iteration_count; ++iteration) {
    auto samples = sample_clock_n(clock, sample_count);
    auto decreasing_it = std::adjacent_find(
      samples.begin(),
      samples.end(),
      [&clock](const sample_type& x, const sample_type& y) noexcept->bool {
        return !(clock.make_time_point(x) <= clock.make_time_point(y));
      });
    EXPECT_EQ(decreasing_it, samples.end());
  }
}

template<class Clock>
class test_real_clock : public testing::Test
{
public:
  using clock_type = Clock;
};

using test_real_clock_types = ::testing::Types<
#if CXXTRACE_HAVE_MACH_TIME
  cxxtrace::apple_absolute_time_clock,
  cxxtrace::apple_approximate_time_clock,
#endif
#if CXXTRACE_HAVE_CLOCK_GETTIME
  cxxtrace::posix_clock_gettime_clock<CLOCK_MONOTONIC>,
#endif
  cxxtrace::posix_gettimeofday_clock,
  cxxtrace::std_high_resolution_clock,
  cxxtrace::std_steady_clock,
  cxxtrace::std_system_clock>;
TYPED_TEST_CASE(test_real_clock, test_real_clock_types, );

TYPED_TEST(test_real_clock, clock_advances_within_decisecond_of_system_clock)
{
  using std::chrono::nanoseconds;
  using clock_type = typename TestFixture::clock_type;
  using reference_clock_type = std::chrono::system_clock;

  static constexpr auto iteration_count = 5;
  static constexpr auto tolerated_mismatches = 1;

  auto test_clock = clock_type{};

  auto test_clock_samples = std::vector<nanoseconds>{};
  auto reference_clock_samples = std::vector<nanoseconds>{};
  for (auto iteration = 0; iteration < iteration_count; ++iteration) {
    if (iteration > 0) {
      std::this_thread::sleep_for(200ms);
    }

    auto test_clock_sample = test_clock.query();
    auto reference_clock_sample = reference_clock_type::now();

    test_clock_samples.emplace_back(
      test_clock.make_time_point(test_clock_sample)
        .nanoseconds_since_reference());
    reference_clock_samples.emplace_back(
      reference_clock_sample.time_since_epoch());
  }

  auto test_clock_deltas = zip_adjacent_to_vector(
    test_clock_samples,
    [](nanoseconds second, nanoseconds first) { return second - first; });
  auto reference_clock_deltas = zip_adjacent_to_vector(
    reference_clock_samples,
    [](auto second, auto first) { return second - first; });
  assert(reference_clock_deltas.size() == test_clock_deltas.size());
  auto delta_differences =
    transform_to_vector(reference_clock_deltas,
                        test_clock_deltas,
                        [](auto x, auto y) { return std::chrono::abs(x - y); });

  auto mismatches =
    std::count_if(delta_differences.begin(),
                  delta_differences.end(),
                  [](nanoseconds difference) { return difference >= 100ms; });
  EXPECT_LE(mismatches, tolerated_mismatches);
}

TEST(test_fake_clock,
     querying_returns_sequence_of_time_points_1_nanosecond_apart)
{
  auto clock = cxxtrace::fake_clock{};
  clock.set_next_time_point(4ns);
  auto samples = sample_clock_n(clock, 6);
  auto queried_times = std::vector<std::chrono::nanoseconds>{};
  std::transform(
    samples.begin(),
    samples.end(),
    std::back_inserter(queried_times),
    [&](cxxtrace::fake_clock::sample sample) {
      return clock.make_time_point(sample).nanoseconds_since_reference();
    });
  EXPECT_THAT(queried_times, ElementsAre(4ns, 5ns, 6ns, 7ns, 8ns, 9ns));
}

TEST(test_fake_clock,
     querying_returns_sequence_of_time_points_configured_duration)
{
  auto clock = cxxtrace::fake_clock{};
  clock.set_duration_between_samples(1098ns);
  clock.set_next_time_point(813ns);
  auto samples = sample_clock_n(clock, 3);
  auto queried_times = std::vector<std::chrono::nanoseconds>{};
  std::transform(
    samples.begin(),
    samples.end(),
    std::back_inserter(queried_times),
    [&](cxxtrace::fake_clock::sample sample) {
      return clock.make_time_point(sample).nanoseconds_since_reference();
    });
  EXPECT_THAT(queried_times,
              ElementsAre(813ns, 813ns + 1098ns, 813ns + 1098ns + 1098ns));
}

namespace {
template<class Clock>
auto
sample_clock_n(Clock& clock, int sample_count) noexcept(false)
  -> std::vector<typename Clock::sample>
{
  using sample_type = typename Clock::sample;
  auto samples = std::vector<sample_type>{};
  samples.resize(sample_count);
  for (auto& sample : samples) {
    sample = clock.query();
  }
  return samples;
}
}
}
