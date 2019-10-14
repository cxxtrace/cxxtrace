#ifndef CXXTRACE_SPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H
#define CXXTRACE_SPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/lazy_thread_local.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/spin_lock.h>
#include <cxxtrace/detail/spsc_ring_queue.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <vector>

namespace cxxtrace {

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
class spsc_ring_queue_processor_local_storage
{
public:
  explicit spsc_ring_queue_processor_local_storage() noexcept(false);
  ~spsc_ring_queue_processor_local_storage() noexcept;

  spsc_ring_queue_processor_local_storage(
    const spsc_ring_queue_processor_local_storage&) = delete;
  spsc_ring_queue_processor_local_storage& operator=(
    const spsc_ring_queue_processor_local_storage&) = delete;
  spsc_ring_queue_processor_local_storage(
    spsc_ring_queue_processor_local_storage&&) = delete;
  spsc_ring_queue_processor_local_storage& operator=(
    spsc_ring_queue_processor_local_storage&&) = delete;

  auto reset() noexcept -> void;

  auto add_sample(detail::sample_site_local_data,
                  ClockSample time_point,
                  thread_id) noexcept -> void;
  auto add_sample(detail::sample_site_local_data,
                  ClockSample time_point) noexcept -> void;
  template<class Clock>
  auto take_all_samples(Clock&) noexcept(false) -> samples_snapshot;
  auto remember_current_thread_name_for_next_snapshot() -> void;

private:
  using sample = detail::global_sample<ClockSample>;

  struct processor_samples
  {
    detail::spin_lock mutex;
    detail::spsc_ring_queue<sample, CapacityPerProcessor> samples;
  };

  using processor_id_lookup_thread_local_cache =
    typename detail::processor_id_lookup::thread_local_cache;

  auto take_remembered_thread_names() -> detail::thread_name_set;

  detail::processor_id_lookup processor_id_lookup;
  std::vector<processor_samples> samples_by_processor;

  std::mutex remembered_thread_names_mutex;
  detail::thread_name_set remembered_thread_names;

  // Synchronizes consuming samples_by_processor[n].processor_samples.samples.
  std::mutex pop_samples_mutex;

  // TODO(strager): Only create this thread-local variable if it's actually used
  // by processor_id_lookup.
  inline static detail::
    lazy_thread_local<processor_id_lookup_thread_local_cache, Tag>
      processor_id_cache;
};
}

#include <cxxtrace/spsc_ring_queue_processor_local_storage_impl.h> // IWYU pragma: export

#endif
