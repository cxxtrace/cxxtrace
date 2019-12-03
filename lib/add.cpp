#include <cassert>
#include <cxxtrace/detail/add.h>
#include <optional>

namespace cxxtrace {
namespace detail {
#if 0
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

#define CXXTRACE_ADD(type)                                                     \
  template auto add<type>(type x, type y) noexcept->std::optional<type>;

CXXTRACE_ADD(int)
CXXTRACE_ADD(long long)
CXXTRACE_ADD(long)
CXXTRACE_ADD(short)
CXXTRACE_ADD(signed char)
CXXTRACE_ADD(unsigned char)
CXXTRACE_ADD(unsigned long long)
CXXTRACE_ADD(unsigned long)
CXXTRACE_ADD(unsigned short)
CXXTRACE_ADD(unsigned)

#undef CXXTRACE_ADD
#endif
}
}
