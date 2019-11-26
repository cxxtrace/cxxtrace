#include "gtest_scoped_trace.h"
#include <cstdint>
#include <cxxtrace/detail/add.h>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <random>
#include <sstream>

using cxxtrace::detail::add;

namespace cxxtrace_test {
TEST(test_add, signed_addition_with_no_overflow)
{
  using value_type = signed char;
  using wide_type = int;
  static_assert(std::numeric_limits<wide_type>::max() >
                std::numeric_limits<value_type>::max());
  auto max = std::numeric_limits<value_type>::max();

  for (auto x = wide_type{ 0 }; x < wide_type{ max }; ++x) {
    for (auto y = wide_type{ 0 }; y < wide_type{ max } - x; ++y) {
      CXXTRACE_SCOPED_TRACE() << "x = " << x << "; y = " << y;
      ASSERT_LE(x + y, wide_type{ max });

      auto maybe_sum =
        add(static_cast<value_type>(x), static_cast<value_type>(y));
      EXPECT_TRUE(maybe_sum.has_value())
        << "Addition should not have overflowed";
      EXPECT_EQ(wide_type{ *maybe_sum }, x + y);

      if (this->HasFailure()) {
        return;
      }
    }
  }
}

TEST(test_add, small_overflow_reports_overflow)
{
  using value_type = signed char;
  static_assert(std::numeric_limits<value_type>::max() == 127);

  EXPECT_EQ(add(value_type{ 127 }, value_type{ 1 }),
            std::optional<value_type>{});
  EXPECT_EQ(add(value_type{ 1 }, value_type{ 127 }),
            std::optional<value_type>{});
  EXPECT_EQ(add(value_type{ 125 }, value_type{ 10 }),
            std::optional<value_type>{});
  EXPECT_EQ(add(value_type{ 125 }, value_type{ 20 }),
            std::optional<value_type>{});
}

TEST(test_add, large_overflow_reports_overflow)
{
  using value_type = signed char;
  static_assert(std::numeric_limits<value_type>::max() == 127);

  EXPECT_EQ(add(value_type{ 127 }, value_type{ 127 }),
            std::optional<value_type>{});
}

template<class T>
class test_add_using_type : public testing::Test
{
public:
  using value_type = T;
};

using test_add_using_type_types =
  testing::Types<signed char, unsigned char, int, unsigned>;

TYPED_TEST_CASE(test_add_using_type, test_add_using_type_types, );

TYPED_TEST(test_add_using_type, random)
{
  using value_type = typename TestFixture::value_type;
  static constexpr auto max = std::numeric_limits<value_type>::max();

  using wide_type = std::intmax_t;
  static_assert(std::numeric_limits<wide_type>::max() > max);

  auto iterations = 10'000;

  auto rng = std::mt19937{};
  auto x_distribution = std::uniform_int_distribution<value_type>{ 0, max };
  auto y_distribution = std::uniform_int_distribution<value_type>{ 0, max };
  for (auto i = 0; i < iterations; ++i) {
    auto x = x_distribution(rng);
    auto y = y_distribution(rng);
    CXXTRACE_SCOPED_TRACE()
      << "x = " << wide_type{ x } << "; y = " << wide_type{ y };

    auto wide_sum = wide_type{ x } + wide_type{ y };
    auto should_overflow = wide_sum > wide_type{ max };
    auto expected_maybe_sum =
      should_overflow ? std::nullopt
                      : std::optional{ static_cast<value_type>(wide_sum) };

    auto maybe_sum = add(x, y);
    EXPECT_EQ(maybe_sum, expected_maybe_sum);
  }
}
}
