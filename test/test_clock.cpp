#include <algorithm>
#include <chrono>
#include <cmath>
#include <cxxtrace/clock.h>
#include <gtest/gtest.h>
#include <iterator>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace cxxtrace {
namespace {
template<class Clock>
auto
sample_clock_n(Clock&, int sample_count) noexcept(false)
  -> std::vector<typename Clock::sample>;

template<class Iterator, class OutputIterator, class BinaryOperation>
auto
zip_adjacent(Iterator begin,
             Iterator end,
             OutputIterator output_begin,
             BinaryOperation op) -> OutputIterator;

template<class Container, class OutputIterator, class BinaryOperation>
auto
zip_adjacent(Container, OutputIterator output_begin, BinaryOperation op)
  -> OutputIterator;

template<class Container, class BinaryOperation>
auto
zip_adjacent_to_vector(Container container, BinaryOperation op) -> std::vector<
  std::decay_t<decltype(op(*std::begin(container), *std::begin(container)))>>;

template<class Container1, class Container2, class BinaryOperation>
auto
transform_to_vector(Container1 container_1,
                    Container2 container_2,
                    BinaryOperation op)
  -> std::vector<std::decay_t<decltype(op(*std::begin(container_1),
                                          *std::begin(container_2)))>>;
}

template<class Clock>
class test_strictly_increasing_clock : public testing::Test
{
public:
  using clock_type = Clock;
};

using test_strictly_increasing_clock_types =
  ::testing::Types<apple_absolute_time_clock, fake_clock>;
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
class test_real_clock : public testing::Test
{
public:
  using clock_type = Clock;
};

using test_real_clock_types = ::testing::Types<apple_absolute_time_clock>;
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

template<class Iterator, class OutputIterator, class BinaryOperation>
auto
zip_adjacent(Iterator begin,
             Iterator end,
             OutputIterator output_begin,
             BinaryOperation op) -> OutputIterator
{
  assert(begin != end);
  return std::transform(std::next(begin), end, begin, output_begin, op);
}

template<class Container, class OutputIterator, class BinaryOperation>
auto
zip_adjacent(Container container,
             OutputIterator output_begin,
             BinaryOperation op) -> OutputIterator
{
  return zip_adjacent(
    std::begin(container), std::end(container), output_begin, op);
}

template<class Container, class BinaryOperation>
auto
zip_adjacent_to_vector(Container container, BinaryOperation op) -> std::vector<
  std::decay_t<decltype(op(*std::begin(container), *std::begin(container)))>>
{
  using output_value_type =
    std::decay_t<decltype(op(*std::begin(container), *std::begin(container)))>;
  auto output = std::vector<output_value_type>{};
  zip_adjacent(container, std::back_inserter(output), op);
  return output;
}

template<class Container1, class Container2, class BinaryOperation>
auto
transform_to_vector(Container1 container_1,
                    Container2 container_2,
                    BinaryOperation op)
  -> std::vector<std::decay_t<decltype(op(*std::begin(container_1),
                                          *std::begin(container_2)))>>
{
  using output_value_type = std::decay_t<decltype(
    op(*std::begin(container_1), *std::begin(container_2)))>;
  auto output = std::vector<output_value_type>{};
  assert(std::size(container_2) >= std::size(container_1));
  std::transform(std::begin(container_1),
                 std::end(container_1),
                 std::begin(container_2),
                 std::back_inserter(output),
                 op);
  return output;
}
}
}
