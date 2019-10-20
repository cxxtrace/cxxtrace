#ifndef CXXTRACE_DETAIL_PROCESSOR_LOCAL_MPSC_RING_QUEUE_H
#define CXXTRACE_DETAIL_PROCESSOR_LOCAL_MPSC_RING_QUEUE_H

// TODO(strager): Support processor_local_mpsc_ring_queue for production code
// (outside model checkers).
#if CXXTRACE_ENABLE_CONCURRENCY_STRESS || CXXTRACE_ENABLE_RELACY
#define CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE 1
#endif

#if CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE
#include <cxxtrace/detail/spsc_ring_queue.h>
#endif

namespace cxxtrace {
namespace detail {
enum class processor_local_mpsc_ring_queue_push_result : bool
{
  push_interrupted_due_to_preemption = false,
  pushed = true,
};

#if CXXTRACE_ENABLE_PROCESSOR_LOCAL_MPSC_RING_QUEUE
template<class T,
         std::size_t Capacity,
         class Index = int,
         class Sync = real_synchronization>
class processor_local_mpsc_ring_queue
{
public:
  using size_type = Index;
  using value_type = T;
  using push_result = processor_local_mpsc_ring_queue_push_result;

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

  preempted:
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
  struct processor_queue
    : public spsc_ring_queue<value_type, capacity, size_type, Sync>
  {};

  std::vector<processor_queue> queue_by_processor;
};
#endif
}
}

#endif
