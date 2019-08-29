#ifndef CXXTRACE_SPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H
#define CXXTRACE_SPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/lazy_thread_local.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/spin_lock.h>
#include <cxxtrace/detail/spmc_ring_queue.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
class spmc_ring_queue_processor_local_storage
{
public:
  explicit spmc_ring_queue_processor_local_storage() noexcept(false);
  ~spmc_ring_queue_processor_local_storage() noexcept;

  spmc_ring_queue_processor_local_storage(
    const spmc_ring_queue_processor_local_storage&) = delete;
  spmc_ring_queue_processor_local_storage& operator=(
    const spmc_ring_queue_processor_local_storage&) = delete;
  spmc_ring_queue_processor_local_storage(
    spmc_ring_queue_processor_local_storage&&) = delete;
  spmc_ring_queue_processor_local_storage& operator=(
    spmc_ring_queue_processor_local_storage&&) = delete;

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
    detail::spmc_ring_queue<sample, CapacityPerProcessor> samples;
  };

  using processor_id_lookup_thread_local_cache =
    typename detail::processor_id_lookup::thread_local_cache;

  detail::processor_id_lookup processor_id_lookup;
  std::vector<processor_samples> samples_by_processor;
  detail::thread_name_set remembered_thread_names;

  // TODO(strager): Only create this thread-local variable if it's actually used
  // by processor_id_lookup.
  inline static detail::
    lazy_thread_local<processor_id_lookup_thread_local_cache, Tag>
      processor_id_cache;
};
}

#include <cxxtrace/spmc_ring_queue_processor_local_storage_impl.h> // IWYU pragma: export

#endif
