#ifndef CXXTRACE_DETAIL_STRING_H
#define CXXTRACE_DETAIL_STRING_H

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <exception>
#include <limits>
#include <system_error>
#include <type_traits>
#include <utility>

namespace cxxtrace {
namespace detail {
template<class T, class = void>
struct stringifier;

template<class... Arg>
auto
stringify(Arg&&... args) noexcept -> auto
{
  constexpr auto max_length = (stringifier<Arg>::max_length + ...);
  std::array<char, max_length + 1> buffer /* uninitialized */;
  auto* buffer_end = buffer.data() + buffer.size();

  auto* p = buffer.data();
  ((p = stringifier<Arg>::append(std::forward<Arg>(args), p, buffer_end)), ...);
  assert(p < buffer_end);
  *p = '\0';

  return buffer;
}

// Convert a C++ string literal to a string.
template<std::size_t Size>
struct stringifier<const char (&)[Size]>
{
  static constexpr auto max_length = std::size_t{ Size - 1 };

  static auto append(const char (&x)[Size],
                     char* out,
                     [[maybe_unused]] char* buffer_end) noexcept -> char*
  {
    assert(buffer_end >= out);
    assert(std::size_t(buffer_end - out) >= max_length);
    assert(x[Size - 1] == '\0');
    auto* x_end = std::find(x, x + Size, '\0');
    return std::copy(x, x_end, out);
  }
};

// Convert an integer (e.g. int or unsigned char) to a string.
template<class T>
struct stringifier<T, std::enable_if_t<std::is_integral_v<T>>>
{
  using limits = std::numeric_limits<T>;
  static constexpr auto max_length = std::size_t{ limits::digits10 + 1 + 1 };

  static auto append(T x, char* out, char* buffer_end) noexcept -> char*
  {
    assert(buffer_end >= out);
    assert(std::size_t(buffer_end - out) >= max_length);
    auto result = std::to_chars(out, buffer_end, x);
    assert(result.ec == std::errc{});
    if (result.ec != std::errc{}) {
      std::fprintf(
        stderr, "fatal: error converting integer to string: buffer to small\n");
      std::terminate();
    }
    auto* end = result.ptr;

#if CXXTRACE_WORK_AROUND_LIBCXX_42166
    {
      auto* digits_begin = out + (x < 0 ? 1 : 0);
      end = strip_leading_zeroes(digits_begin, end);
    }
#endif

    return end;
  }

#if CXXTRACE_WORK_AROUND_LIBCXX_42166
  static auto strip_leading_zeroes(char* begin, char* end) noexcept -> char*
  {
    assert(end >= begin);
    auto* new_begin = std::find_if(begin, end, [](char c) { return c != '0'; });
    if (new_begin == end) {
      assert(end - begin == 1);
      assert(*begin == '0');
      return end;
    }

    auto new_length = end - new_begin;
    auto* new_end = begin + new_length;
    std::copy_backward(new_begin, end, new_end);
    return new_end;
  }
#endif
};

// Convert an integer (e.g. int&) to a string.
template<class T>
struct stringifier<T&, std::enable_if_t<std::is_integral_v<T>>>
  : public stringifier<T>
{};
}
}

#endif
