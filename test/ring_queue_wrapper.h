#ifndef CXXTRACE_RING_QUEUE_WRAPPER_H
#define CXXTRACE_RING_QUEUE_WRAPPER_H

#include "processor_local_mpsc_ring_queue.h"
#include "void_t.h"
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/queue_sink.h>
#include <type_traits>

#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY
#include "cxxtrace_concurrency_test.h"
#else
#include <cstdio>
#include <exception>
#endif

#if CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE
#include "rseq_scheduler.h"
#endif

namespace cxxtrace_test {
namespace { // Avoid ODR violations due to #if.
template<class RingQueue, class = void_t<>>
class ring_queue_wrapper;

template<class RingQueue>
class ring_queue_wrapper<
  RingQueue,
  void_t<decltype(std::declval<RingQueue&>().push(1, nullptr))>>
{
public:
  using push_result = cxxtrace::detail::mpsc_ring_queue_push_result;
  using size_type = typename RingQueue::size_type;
  using value_type = typename RingQueue::value_type;

  static inline constexpr const auto capacity = RingQueue::capacity;

  template<class... Args>
  explicit ring_queue_wrapper(Args&&... args)
    : queue(std::forward<Args>(args)...)
  {}

  auto reset() noexcept -> void { this->queue.reset(); }

  template<class WriterFunction>
  auto push(size_type count, WriterFunction&& write) noexcept -> void
  {
    this->queue.push(count, std::forward<WriterFunction>(write));
  }

  template<class WriterFunction>
  auto try_push(size_type count, WriterFunction&& write) noexcept -> push_result
  {
    this->push(count, std::forward<WriterFunction>(write));
    return push_result::pushed;
  }

  template<class Allocator>
  auto pop_all_into(std::vector<value_type, Allocator>& output) -> void
  {
    this->pop_all_into(
      cxxtrace::detail::vector_queue_sink<value_type, Allocator>{ output });
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    this->queue.pop_all_into(std::forward<Sink>(output));
  }

  RingQueue queue;
};

template<class RingQueue>
class ring_queue_wrapper<
  RingQueue,
  void_t<decltype(std::declval<RingQueue&>().try_push(1, nullptr))>>
{
public:
  using push_result = typename RingQueue::push_result;
  using size_type = typename RingQueue::size_type;
  using value_type = typename RingQueue::value_type;

  static inline constexpr const auto capacity = RingQueue::capacity;

  template<class... Args>
  explicit ring_queue_wrapper(Args&&... args)
    : queue(std::forward<Args>(args)...)
  {}

  auto reset() noexcept -> void { this->queue.reset(); }

  template<class WriterFunction>
  auto push(size_type count, WriterFunction&& write) noexcept -> void
  {
    auto push = [&] {
      auto result = this->try_push(count, std::forward<WriterFunction>(write));
      this->assert_push_succeeded(result);
    };
#if CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE
    auto* scheduler = rseq_scheduler<concurrency_test_synchronization>::get();
    if (scheduler) {
      auto guard = scheduler->disable_preemption(CXXTRACE_HERE);
      push();
    } else {
      push();
    }
#else
    push();
#endif
  }

  template<class WriterFunction>
  auto try_push(size_type count, WriterFunction&& write) noexcept -> push_result
  {
    return this->queue.try_push(count, std::forward<WriterFunction>(write));
  }

  template<class Allocator>
  auto pop_all_into(std::vector<value_type, Allocator>& output) -> void
  {
    this->pop_all_into(
      cxxtrace::detail::vector_queue_sink<value_type, Allocator>{ output });
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    this->queue.pop_all_into(std::forward<Sink>(output));
  }

  RingQueue queue;

private:
  static auto assert_push_succeeded(
    cxxtrace::detail::mpsc_ring_queue_push_result push_result) noexcept -> void
  {
    using cxxtrace::detail::mpsc_ring_queue_push_result;
    switch (push_result) {
      case mpsc_ring_queue_push_result::not_pushed_due_to_contention:
#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY
        CXXTRACE_ASSERT(false);
#else
        std::fprintf(stderr, "fatal: try_push failed due to contention\n");
        std::terminate();
#endif
        break;
      case mpsc_ring_queue_push_result::pushed:
        break;
    }
  }

  static auto assert_push_succeeded(
    cxxtrace_test::processor_local_mpsc_ring_queue_push_result push_result)
    -> void
  {
    using cxxtrace_test::processor_local_mpsc_ring_queue_push_result;
    switch (push_result) {
      case processor_local_mpsc_ring_queue_push_result::
        push_interrupted_due_to_preemption:
#if CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY
        CXXTRACE_ASSERT(false);
#else
        std::fprintf(stderr, "fatal: try_push failed due to interrupt\n");
        std::terminate();
#endif
        break;
      case processor_local_mpsc_ring_queue_push_result::pushed:
        break;
    }
  }
};
}
}

#endif
