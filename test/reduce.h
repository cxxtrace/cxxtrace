#ifndef CXXTRACE_TEST_REDUCE_H
#define CXXTRACE_TEST_REDUCE_H

#include <algorithm>
#include <numeric>

#if defined(__GLIBCXX__)
// libstdc++ version 9.1.0 does not define some overloads of
// std::reduce.
#define CXXTRACE_WORK_AROUND_STD_REDUCE 1
#endif

#if CXXTRACE_WORK_AROUND_STD_REDUCE
#include <iterator>
#endif

namespace cxxtrace_test {
template<class Iterator>
auto
reduce(Iterator begin, Iterator end) -> auto
{
#if CXXTRACE_WORK_AROUND_STD_REDUCE
  return std::accumulate(
    begin, end, typename std::iterator_traits<Iterator>::value_type());
#else
  return std::reduce(begin, end);
#endif
}
}

#endif
