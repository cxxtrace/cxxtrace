#include "exhaustive_rng.h"
#include "gtest_scoped_trace.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <numeric>
#include <optional>
#include <ostream>
#include <sstream>
#include <utility>
#include <vector>

namespace cxxtrace_test {
TEST(test_exhaustive_rng, rng_is_exhaustive_with_no_calls)
{
  auto rng = exhaustive_rng{};
  rng.lap();
  EXPECT_TRUE(rng.done());
}

TEST(test_exhaustive_rng, next_integer_0_1_is_in_its_own_lap)
{
  auto rng = exhaustive_rng{};
  EXPECT_EQ(rng.next_integer_0(1), 0);
  rng.lap();
  EXPECT_TRUE(rng.done());
}

class test_exhaustive_rng_n : public testing::TestWithParam<int>
{};

INSTANTIATE_TEST_CASE_P(,
                        test_exhaustive_rng_n,
                        testing::Values(16, 256),
                        testing::PrintToStringParamName());

TEST_P(test_exhaustive_rng_n, exhaust_with_one_next_n_call_per_lap)
{
  auto number_count = this->GetParam();

  auto expected_numbers = std::vector<int>{};
  expected_numbers.resize(number_count);
  std::iota(expected_numbers.begin(), expected_numbers.end(), 0);

  auto rng = exhaustive_rng{};
  auto lap_count = 0;
  while (lap_count < number_count) {
    CXXTRACE_SCOPED_TRACE() << "lap " << lap_count;
    auto actual_number = rng.next_integer_0(number_count);
    EXPECT_EQ(actual_number, expected_numbers[lap_count]);
    rng.lap();
    lap_count += 1;
    if (rng.done()) {
      break;
    }
  }
  EXPECT_EQ(lap_count, number_count);
  EXPECT_TRUE(rng.done()) << "rng should be done after " << number_count
                          << " laps";
}

TEST(test_exhaustive_rng, exhaust_with_two_next_256_calls_per_lap)
{
  auto expected_numbers = std::vector<std::pair<int, int>>{};
  for (auto i = 0; i < 256; ++i) {
    for (auto j = 0; j < 256; ++j) {
      expected_numbers.emplace_back(i, j);
    }
  }

  auto rng = exhaustive_rng{};
  auto lap_count = 0;
  while (lap_count < 256 * 256) {
    CXXTRACE_SCOPED_TRACE() << "lap " << lap_count;
    auto actual_number_0 = rng.next_integer_0(256);
    auto actual_number_1 = rng.next_integer_0(256);
    EXPECT_EQ((std::pair{ actual_number_0, actual_number_1 }),
              expected_numbers[lap_count]);
    rng.lap();
    lap_count += 1;
    if (rng.done()) {
      break;
    }
    if (this->HasFailure()) {
      return;
    }
  }
  EXPECT_EQ(lap_count, expected_numbers.size());
  EXPECT_TRUE(rng.done()) << "rng should be done after " << 256 * 256
                          << " laps";
}

TEST(test_exhaustive_rng, first_few_laps_with_one_or_two_next256_calls_per_lap)
{
  auto rng = exhaustive_rng{};

  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 1);
  rng.lap();

  for (auto i = 2; i < 256; ++i) {
    rng.next_integer_0(256);
    rng.next_integer_0(256);
    rng.lap();
  }

  EXPECT_EQ(rng.next_integer_0(256), 1);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 2);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 3);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 4);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 4);
  EXPECT_EQ(rng.next_integer_0(256), 1);
}

TEST(test_exhaustive_rng,
     first_few_laps_with_one_next16_call_and_maybe_one_next256_call_per_lap)
{
  auto rng = exhaustive_rng{};

  EXPECT_EQ(rng.next_integer_0(16), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(16), 1);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(16), 1);
  EXPECT_EQ(rng.next_integer_0(256), 1);
  rng.lap();

  for (auto i = 2; i < 256; ++i) {
    rng.next_integer_0(16);
    rng.next_integer_0(256);
    rng.lap();
  }

  EXPECT_EQ(rng.next_integer_0(16), 2);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(16), 3);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(16), 4);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(16), 5);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 5);
  EXPECT_EQ(rng.next_integer_0(256), 1);
}

TEST(test_exhaustive_rng, exhaust_with_one_or_two_next256_calls_per_lap)
{
  auto expected_numbers = std::vector<std::pair<int, std::optional<int>>>{};
  for (auto i = 0; i < 256; ++i) {
    if (i % 3 == 0) {
      for (auto j = 0; j < 256; ++j) {
        expected_numbers.emplace_back(i, j);
      }
    } else {
      expected_numbers.emplace_back(i, std::nullopt);
    }
  }

  auto rng = exhaustive_rng{};
  auto lap_count = std::size_t{ 0 };
  while (lap_count < expected_numbers.size()) {
    CXXTRACE_SCOPED_TRACE() << "lap " << lap_count;
    auto actual_number_0 = rng.next_integer_0(256);
    auto actual_number_1 = std::optional<int>{};
    if (expected_numbers[lap_count].second.has_value()) {
      actual_number_1 = rng.next_integer_0(256);
    }
    EXPECT_EQ((std::pair{ actual_number_0, actual_number_1 }),
              expected_numbers[lap_count]);
    rng.lap();
    lap_count += 1;
    if (rng.done()) {
      break;
    }
  }
  EXPECT_EQ(lap_count, expected_numbers.size());
  EXPECT_TRUE(rng.done()) << "rng should be done after "
                          << expected_numbers.size() << " laps";
}

TEST(test_exhaustive_rng, first_few_laps_with_three_next256_calls_per_lap)
{
  auto rng = exhaustive_rng{};

  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 1);
  rng.lap();

  for (auto i = 2; i < 256; ++i) {
    rng.next_integer_0(256);
    rng.next_integer_0(256);
    rng.next_integer_0(256);
    rng.lap();
  }

  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 1);
  EXPECT_EQ(rng.next_integer_0(256), 0);
  rng.lap();

  EXPECT_EQ(rng.next_integer_0(256), 0);
  EXPECT_EQ(rng.next_integer_0(256), 1);
  EXPECT_EQ(rng.next_integer_0(256), 1);
  rng.lap();
}

TEST(test_exhaustive_rng, exhaust_with_three_next32_calls_per_lap)
{
  auto expected_numbers_for_lap =
    [](int lap) noexcept->std::tuple<int, int, int>
  {
    return std::tuple{ (lap >> 10) & 0x1f, (lap >> 5) & 0x1f, lap & 0x1f };
  };
  auto expected_laps = 32 * 32 * 32;
  auto rng = exhaustive_rng{};
  auto lap_count = 0;
  while (lap_count < expected_laps) {
    CXXTRACE_SCOPED_TRACE() << "lap " << lap_count;
    auto actual_number_0 = rng.next_integer_0(32);
    auto actual_number_1 = rng.next_integer_0(32);
    auto actual_number_2 = rng.next_integer_0(32);
    auto actual_numbers =
      std::tuple{ actual_number_0, actual_number_1, actual_number_2 };
    auto expected_numbers = expected_numbers_for_lap(lap_count);
    EXPECT_EQ(actual_numbers, expected_numbers);
    rng.lap();
    lap_count += 1;
    if (rng.done()) {
      break;
    }
  }
  EXPECT_EQ(lap_count, expected_laps);
  EXPECT_TRUE(rng.done()) << "rng should be done after " << expected_laps
                          << " laps";
}
}
