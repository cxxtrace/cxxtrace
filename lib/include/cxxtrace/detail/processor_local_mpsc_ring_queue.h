#ifndef CXXTRACE_DETAIL_PROCESSOR_LOCAL_MPSC_RING_QUEUE_H
#define CXXTRACE_DETAIL_PROCESSOR_LOCAL_MPSC_RING_QUEUE_H

#include "/home/strager/Projects/cxxtrace/test/rseq_scheduler.h" // @@@
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/spsc_ring_queue.h>

// @@@ we need to inject allow_reempt calls in spsc_ring_queue.

namespace cxxtrace {
namespace detail {
template<class T,
         std::size_t Capacity,
         class Index = int,
         class Sync = real_synchronization>
class processor_local_mpsc_ring_queue
{
public:
  using size_type = Index;
  using value_type = T;
  using push_result = mpsc_ring_queue_push_result; /// @@@ separate enum?

  static inline constexpr const auto capacity = size_type{ Capacity };

  auto reset() noexcept -> void
  {
    for (auto& queue : this->queue_by_processor) {
      queue.reset();
    }
  }

  template<class WriterFunction>
  auto try_push(size_type count, WriterFunction&& write) noexcept -> push_result
  {
    auto& rseq = cxxtrace_test::rseq_scheduler<Sync>::get();
    CXXTRACE_BEGIN_PREEMPTABLE(rseq, preempted);
    {
      auto processor_id = rseq.get_current_processor_id(CXXTRACE_HERE);
      assert(processor_id < this->queue_by_processor.size());
      auto& queue = this->queue_by_processor[processor_id];

      queue.push(count, std::forward<WriterFunction>(write));
    }
    rseq.end_preemptable(CXXTRACE_HERE);
    return push_result::pushed;

  preempted:
    return push_result::push_interrupted_due_to_preemption;
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    // @@@ items will be unordered!
    for (auto& queue : this->queue_by_processor) {
      queue.pop_all_into(std::forward<Sink>(output));
    }
  }

private:
  struct processor_queue
    : public spsc_ring_queue<value_type, capacity, size_type, Sync>
  {};

  static constexpr auto processor_count = 4; // @@@ obviously wrong

  std::vector<processor_queue> queue_by_processor{ processor_count };
};
}
}

#endif
