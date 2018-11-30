#ifndef CXXTRACE_DETAIL_VECTOR_H
#define CXXTRACE_DETAIL_VECTOR_H

#include <utility>

namespace cxxtrace {
namespace detail {
template<class Vector>
auto
reset_vector(Vector& vector) -> void
{
  std::exchange(vector, Vector{});
}
}
}

#endif
