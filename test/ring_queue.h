#ifndef CXXTRACE_RING_QUEUE_H
#define CXXTRACE_RING_QUEUE_H

#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY
#include "cxxtrace_concurrency_test.h"
#else
#include <cstdio>
#include <exception>
#endif

#include <type_traits>

namespace cxxtrace_test {
namespace detail {
template<class RingQueue, class = std::void_t<>>
struct has_try_push : public std::false_type
{};

template<class RingQueue>
struct has_try_push<
  RingQueue,
  std::void_t<decltype(std::declval<RingQueue&>().try_push(1, nullptr))>>
  : public std::true_type
{};
}

namespace { // Avoid ODR violations due to #if.
template<class RingQueue, class WriterFunction>
auto
push(RingQueue& queue,
     typename RingQueue::size_type size,
     WriterFunction&& write) -> void
{
  if constexpr (detail::has_try_push<RingQueue>::value) {
    auto result = queue.try_push(size, std::forward<WriterFunction>(write));
    switch (result) {
      case RingQueue::push_result::not_pushed_due_to_contention:
#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY
        CXXTRACE_ASSERT(false);
#else
        std::fprintf(stderr, "fatal: try_push failed due to contention\n");
        std::terminate();
#endif
        break;
      case RingQueue::push_result::pushed:
        break;
    }
  } else {
    queue.push(size, std::forward<WriterFunction>(write));
  }
}
}
}

#endif
