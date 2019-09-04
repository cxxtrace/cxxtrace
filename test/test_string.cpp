#include "stringify.h"
#include <cxxtrace/detail/string.h>
#include <gtest/gtest.h>
#include <limits>

namespace cxxtrace_test {
TEST(test_stringify, string_integer_string)
{
#define CHECK(arg_0, arg_1, arg_2)                                             \
  EXPECT_STREQ(cxxtrace::detail::stringify(arg_0, arg_1, arg_2).data(),        \
               cxxtrace_test::stringify(arg_0, arg_1, arg_2).c_str());

  CHECK("", int{ 0 }, "");
  CHECK("", int{ 1 }, "");
  CHECK("", int{ -1 }, "");
  CHECK("", int{ 10000 }, "");
  CHECK("", std::numeric_limits<int>::max(), "");
  CHECK("", std::numeric_limits<int>::lowest(), "");

  CHECK("hello", int{ 0 }, "");
  CHECK("", int{ 0 }, "world");
  CHECK("hello", int{ 0 }, "world");

  CHECK("embedded\0null", int{ 42 }, "embedded\0null");

#define CHECK_MIN_MAX_ZERO(arg_0, arg_1_type, arg_2)                           \
  CHECK(arg_0, std::numeric_limits<arg_1_type>::lowest(), arg_2);              \
  CHECK(arg_0, std::numeric_limits<arg_1_type>::max(), arg_2);                 \
  CHECK(arg_0, static_cast<arg_1_type>(0), arg_2);

  // NOTE(strager): Don't check bool or char. std::ostream doesn't treat those
  // types as integer types.
  CHECK_MIN_MAX_ZERO("[", int, "]");
  CHECK_MIN_MAX_ZERO("[", long long, "]");
  CHECK_MIN_MAX_ZERO("[", long, "]");
  CHECK_MIN_MAX_ZERO("[", short, "]");
  CHECK_MIN_MAX_ZERO("[", unsigned long, "]");
  CHECK_MIN_MAX_ZERO("[", unsigned long long, "]");
  CHECK_MIN_MAX_ZERO("[", unsigned short, "]");
  CHECK_MIN_MAX_ZERO("[", unsigned, "]");

#undef CHECK_MIN_MAX

#undef CHECK
}

TEST(test_stringify, libcxx_to_chars_leading_zeroes)
{
  // In libc++ 8.0.0, std::to_chars writes leading zeros for large integers.
  // (See bug report: https://bugs.llvm.org/show_bug.cgi?id=42166) Verify that
  // stringify does *not* include leading zeros.

#define CHECK(arg_0)                                                           \
  EXPECT_STREQ(cxxtrace::detail::stringify(arg_0##ULL).data(), #arg_0);        \
  EXPECT_STREQ(cxxtrace::detail::stringify(arg_0##LL).data(), #arg_0);         \
  EXPECT_STREQ(cxxtrace::detail::stringify(-arg_0##LL).data(), "-" #arg_0);

  CHECK(1073741824);
  CHECK(134217728);
  CHECK(137438953472);
  CHECK(17179869184);
  CHECK(2147483648);
  CHECK(268435456);
  CHECK(274877906944);
  CHECK(34359738368);
  CHECK(4294967296);
  CHECK(536870912);
  CHECK(549755813888);
  CHECK(68719476736);
  CHECK(8589934592);

#undef CHECK
}
}
