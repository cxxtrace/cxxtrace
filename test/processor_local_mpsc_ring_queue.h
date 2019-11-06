#ifndef CXXTRACE_TEST_PROCESSOR_LOCAL_MPSC_RING_QUEUE_H
#define CXXTRACE_TEST_PROCESSOR_LOCAL_MPSC_RING_QUEUE_H

#include <cxxtrace/detail/workarounds.h>

// TODO(strager): Support processor_local_mpsc_ring_queue for production code
// (outside model checkers).
#if (CXXTRACE_ENABLE_CONCURRENCY_STRESS || CXXTRACE_ENABLE_CDSCHECKER ||       \
     CXXTRACE_ENABLE_RELACY) &&                                                \
  !CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
#define CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE 1
#endif

#if CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE
#include "rseq_scheduler.h"
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/real_synchronization.h>
#include <cxxtrace/detail/spsc_ring_queue.h>
#include <utility>
#include <vector>
#endif

namespace cxxtrace_test {
enum class processor_local_mpsc_ring_queue_push_result : bool
{
  push_interrupted_due_to_preemption = false,
  pushed = true,
};

#if CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE
// A special-purpose, lossy, bounded, partially-ordered MPSC container optimized
// for writes.
//
// Special-purpose: Items in a processor_local_mpsc_ring_queue_push_result must
// be trivial. Constructors and destructors are not called on a
// processor_local_mpsc_ring_queue_push_result's items.
//
// Lossy: If a writer pushes too many items, older items are discarded.
//
// Bounded: The maximum number of items allowed in a
// processor_local_mpsc_ring_queue_push_result is fixed. Operations on a
// processor_local_mpsc_ring_queue_push_result will never allocate memory.
//
// Partially-ordered: For a given processor, items pushed by code executing on
// that processor are ordered first in, last out.
//
// MPSC: Zero or more threads can push items ("Multiple Producer"), and one
// thread can pop items ("Single Consumer").
//
// TODO(strager): Add an API for the reader to detect when items are discarded.
//
// TODO(strager): Add an API to query the processor number of popped items.
//
// @see spsc_ring_queue
// @see mpsc_ring_queue
template<class T,
         std::size_t Capacity,
         class Index = int,
         class Sync = cxxtrace::detail::real_synchronization>
class processor_local_mpsc_ring_queue
{
public:
  using size_type = Index;
  using value_type = T;
  using push_result = processor_local_mpsc_ring_queue_push_result;

  // TODO(strager): Capacity is a misleading term. The actual capacity of a
  // queue is (Capacity * processor_count). Switch to a better name.
  static inline constexpr const auto capacity = size_type{ Capacity };

  explicit processor_local_mpsc_ring_queue(int processor_count)
    : queue_by_processor{ static_cast<std::size_t>(processor_count) }
  {}

  auto reset() noexcept -> void
  {
    for (auto& queue : this->queue_by_processor) {
      queue.reset();
    }
  }

  template<class WriterFunction>
  auto try_push(size_type count, WriterFunction&& write) noexcept -> push_result
  {
    auto rseq = Sync::get_rseq();
    CXXTRACE_BEGIN_PREEMPTABLE(rseq, preempted)
      auto processor_id = rseq.get_current_processor_id(CXXTRACE_HERE);
      assert(processor_id < this->queue_by_processor.size());
      auto& queue = this->queue_by_processor[processor_id];
      queue.push(count, std::forward<WriterFunction>(write));
    CXXTRACE_END_PREEMPTABLE(rseq, preempted)
    return push_result::pushed;

    CXXTRACE_ON_PREEMPT(rseq, preempted)
    return push_result::push_interrupted_due_to_preemption;
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    for (auto& queue : this->queue_by_processor) {
      queue.pop_all_into(std::forward<Sink>(output));
    }
  }

private:
  using processor_queue =
    cxxtrace::detail::spsc_ring_queue<value_type, capacity, size_type, Sync>;

  std::vector<processor_queue> queue_by_processor;
};
#endif
}

#endif
