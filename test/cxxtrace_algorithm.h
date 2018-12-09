#ifndef CXXTRACE_ALGORITHM_H
#define CXXTRACE_ALGORITHM_H

#include <iterator>
#include <type_traits>
#include <vector>

namespace cxxtrace {
template<class Container, class BinaryOperation>
auto
zip_adjacent_to_vector(Container container, BinaryOperation op) -> std::vector<
  std::decay_t<decltype(op(*std::begin(container), *std::begin(container)))>>;

template<class Iterator, class OutputIterator, class BinaryOperation>
auto
zip_adjacent(Iterator begin,
             Iterator end,
             OutputIterator output_begin,
             BinaryOperation op) -> OutputIterator;

template<class Container, class UnaryOperation>
auto
transform_to_vector(Container container, UnaryOperation op)
  -> std::vector<std::decay_t<decltype(op(*std::begin(container)))>>;

template<class Container1, class Container2, class BinaryOperation>
auto
transform_to_vector(Container1 container_1,
                    Container2 container_2,
                    BinaryOperation op)
  -> std::vector<std::decay_t<decltype(op(*std::begin(container_1),
                                          *std::begin(container_2)))>>;
}

#include "cxxtrace_algorithm_impl.h"

#endif
