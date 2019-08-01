#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/molecular.h>
#include <gtest/gtest.h>
#include <iterator>
#include <type_traits>

using cxxtrace::detail::largest_lock_free_atomic_value_type;
using cxxtrace::detail::molecular;

#if defined(__LP64__)

#if ATOMIC_CHAR_LOCK_FREE >= 1
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<1>, unsigned char>);
#else
#error "Unsupported platform"
#endif

#if ATOMIC_SHORT_LOCK_FREE >= 1
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<2>, unsigned short>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<3>, unsigned short>);
#else
#error "Unsupported platform"
#endif

#if ATOMIC_INT_LOCK_FREE >= 1
static_assert(std::is_same_v<largest_lock_free_atomic_value_type<4>, unsigned>);
static_assert(std::is_same_v<largest_lock_free_atomic_value_type<5>, unsigned>);
static_assert(std::is_same_v<largest_lock_free_atomic_value_type<6>, unsigned>);
static_assert(std::is_same_v<largest_lock_free_atomic_value_type<7>, unsigned>);
#else
#error "Unsupported platform"
#endif

#if ATOMIC_LONG_LOCK_FREE >= 1
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<8>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<9>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<10>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<11>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<12>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<13>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<14>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<15>, unsigned long>);

static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<16>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<17>, unsigned long>);
static_assert(
  std::is_same_v<largest_lock_free_atomic_value_type<18>, unsigned long>);
#else
#error "Unsupported platform"
#endif

#else
#error "Unsupported platform"
#endif

namespace cxxtrace_test {
template<class T>
class test_molecular_integral : public testing::Test
{
public:
  using integral_type = T;
};
using test_molecular_integral_types = testing::Types<signed char,
                                                     unsigned char,
                                                     char,
                                                     short,
                                                     unsigned short,
                                                     int,
                                                     unsigned,
                                                     long,
                                                     unsigned long,
                                                     long long,
                                                     unsigned long long>;

TYPED_TEST_CASE(test_molecular_integral, test_molecular_integral_types, );

TYPED_TEST(test_molecular_integral, integral_type_round_trips)
{
  using integral_type = typename TestFixture::integral_type;
  auto molecule = molecular<integral_type>{};

  molecule.store(42, CXXTRACE_HERE);
  EXPECT_EQ(molecule.load(CXXTRACE_HERE), 42);

  auto max = std::numeric_limits<integral_type>::max();
  molecule.store(max, CXXTRACE_HERE);
  EXPECT_EQ(molecule.load(CXXTRACE_HERE), max);

  auto min = std::numeric_limits<integral_type>::min();
  molecule.store(min, CXXTRACE_HERE);
  EXPECT_EQ(molecule.load(CXXTRACE_HERE), min);
}

TEST(test_molecular, large_struct_round_trips)
{
  struct large
  {
    int ints[42];
    char chars[9];
    double dub;
    void* pointer;
  };

  auto pointee = nullptr;
  auto molecule = molecular<large>{};
  molecule.store(large{ { 1, 2, 3 }, { 4, 5, 6 }, 7.8, &pointee },
                 CXXTRACE_HERE);
  auto loaded = molecule.load(CXXTRACE_HERE);
  EXPECT_EQ(loaded.ints[0], 1);
  EXPECT_EQ(loaded.ints[1], 2);
  EXPECT_EQ(loaded.ints[2], 3);
  EXPECT_EQ(loaded.chars[0], 4);
  EXPECT_EQ(loaded.chars[1], 5);
  EXPECT_EQ(loaded.chars[2], 6);
  EXPECT_EQ(loaded.dub, 7.8);
  EXPECT_EQ(loaded.pointer, &pointee);
}

template<class IntegralConstant>
class test_molecular_char_array : public testing::Test
{
public:
  inline static constexpr auto size = IntegralConstant::value;
};
using test_molecular_char_array_types =
  testing::Types<std::integral_constant<std::size_t, 1>,
                 std::integral_constant<std::size_t, 2>,
                 std::integral_constant<std::size_t, 3>,
                 std::integral_constant<std::size_t, 4>,
                 std::integral_constant<std::size_t, 7>,
                 std::integral_constant<std::size_t, 9>,
                 std::integral_constant<std::size_t, 15>,
                 std::integral_constant<std::size_t, 17>>;
TYPED_TEST_CASE(test_molecular_char_array, test_molecular_char_array_types, );

TYPED_TEST(test_molecular_char_array, char_array_round_trips)
{
  static constexpr auto size = TestFixture::size;
  static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
  static_assert(sizeof(alphabet) > size);

  auto chars = std::array<char, size>{};
  std::copy_n(std::begin(alphabet), size, chars.begin());

  auto molecule = molecular<std::array<char, size>>{};
  molecule.store(chars, CXXTRACE_HERE);
  EXPECT_EQ(molecule.load(CXXTRACE_HERE), chars);
}

static_assert(std::is_trivial_v<molecular<char>>);
static_assert(std::is_trivial_v<molecular<int>>);
static_assert(std::is_trivial_v<molecular<std::array<long, 42>>>);
}
