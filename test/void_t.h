#ifndef CXXTRACE_VOID_T_H
#define CXXTRACE_VOID_T_H

#include <cxxtrace/detail/workarounds.h>

#if !CXXTRACE_WORK_AROUND_VOID_T_EQUIVALENCE
#include <type_traits>
#endif

namespace cxxtrace_test {
#if CXXTRACE_WORK_AROUND_VOID_T_EQUIVALENCE
namespace detail {
template<class...>
struct to_void
{
  using type = void;
};
}

template<class... T>
using void_t = typename detail::to_void<T...>::type;

#else
using void_t = std::void_t;
#endif
}

#endif
