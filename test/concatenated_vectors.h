#ifndef CXXTRACE_TEST_CONCATENATED_VECTORS_H
#define CXXTRACE_TEST_CONCATENATED_VECTORS_H

#include <experimental/memory_resource>
#include <experimental/vector>

namespace cxxtrace_test {
template<class T>
auto
concatenated_vectors(const std::experimental::pmr::vector<T>& xs,
                     const std::experimental::pmr::vector<T>& ys,
                     std::experimental::pmr::memory_resource* memory)
  -> std::experimental::pmr::vector<T>
{
  auto result = std::experimental::pmr::vector<T>{ memory };
  result.reserve(xs.size() + ys.size());
  result.insert(result.end(), xs.begin(), xs.end());
  result.insert(result.end(), ys.begin(), ys.end());
  return result;
}
}

#endif
