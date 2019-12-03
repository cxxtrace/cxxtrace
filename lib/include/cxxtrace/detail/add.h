#ifndef CXXTRACE_DETAIL_ADD_H
#define CXXTRACE_DETAIL_ADD_H

#include <optional>

namespace cxxtrace {
namespace detail {
// Return `x + y`.
//
// If x + y would overflow (i.e. is not representable as T), instead return
// `std::nullopt`.
//
// x and y must be nonnegative.
template<class T>
auto
add(T x, T y) noexcept -> std::optional<T>;

#if 1
template<class T>
auto
add(T x, T y) noexcept -> std::optional<T>
{
  assert(x >= 0);
  assert(y >= 0);
  auto sum = T{};
  if (__builtin_add_overflow(x, y, &sum)) {
    return std::nullopt;
  }
  return { sum };
}
#else
#define CXXTRACE_EXTERN_ADD(type)                                              \
  extern template auto add<type>(type x, type y) noexcept->std::optional<type>;

CXXTRACE_EXTERN_ADD(int)
CXXTRACE_EXTERN_ADD(long long)
CXXTRACE_EXTERN_ADD(long)
CXXTRACE_EXTERN_ADD(short)
CXXTRACE_EXTERN_ADD(signed char)
CXXTRACE_EXTERN_ADD(unsigned char)
CXXTRACE_EXTERN_ADD(unsigned long long)
CXXTRACE_EXTERN_ADD(unsigned long)
CXXTRACE_EXTERN_ADD(unsigned short)
CXXTRACE_EXTERN_ADD(unsigned)

#undef CXXTRACE_EXTERN_ADD
#endif
}
}

#endif
