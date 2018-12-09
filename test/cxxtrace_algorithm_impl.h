#ifndef CXXTRACE_ALGORITHM_IMPL_H
#define CXXTRACE_ALGORITHM_IMPL_H

#if !defined(CXXTRACE_ALGORITHM_H)
#error                                                                         \
  "Include \"cxxtrace_algorithm.h\" instead of including \"cxxtrace_algorithm_impl.h\" directly."
#endif

#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <vector>

namespace cxxtrace {
template<class Iterator, class OutputIterator, class BinaryOperation>
auto
zip_adjacent(Iterator begin,
             Iterator end,
             OutputIterator output_begin,
             BinaryOperation op) -> OutputIterator
{
  assert(begin != end);
  return std::transform(std::next(begin), end, begin, output_begin, op);
}

template<class Container, class BinaryOperation>
auto
zip_adjacent_to_vector(Container container, BinaryOperation op) -> std::vector<
  std::decay_t<decltype(op(*std::begin(container), *std::begin(container)))>>
{
  using output_value_type =
    std::decay_t<decltype(op(*std::begin(container), *std::begin(container)))>;
  auto output = std::vector<output_value_type>{};
  zip_adjacent(
    std::begin(container), std::end(container), std::back_inserter(output), op);
  return output;
}

template<class Container, class UnaryOperation>
auto
transform_to_vector(Container container, UnaryOperation op)
  -> std::vector<std::decay_t<decltype(op(*std::begin(container)))>>
{
  using output_value_type = std::decay_t<decltype(op(*std::begin(container)))>;
  auto output = std::vector<output_value_type>{};
  std::transform(
    std::begin(container), std::end(container), std::back_inserter(output), op);
  return output;
}

template<class Container1, class Container2, class BinaryOperation>
auto
transform_to_vector(Container1 container_1,
                    Container2 container_2,
                    BinaryOperation op)
  -> std::vector<std::decay_t<decltype(op(*std::begin(container_1),
                                          *std::begin(container_2)))>>
{
  using output_value_type = std::decay_t<decltype(
    op(*std::begin(container_1), *std::begin(container_2)))>;
  auto output = std::vector<output_value_type>{};
  assert(std::size(container_2) >= std::size(container_1));
  std::transform(std::begin(container_1),
                 std::end(container_1),
                 std::begin(container_2),
                 std::back_inserter(output),
                 op);
  return output;
}
}

#endif
