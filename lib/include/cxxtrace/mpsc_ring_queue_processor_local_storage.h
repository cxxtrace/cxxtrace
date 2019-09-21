#ifndef CXXTRACE_MPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H
#define CXXTRACE_MPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/lazy_thread_local.h>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
class mpsc_ring_queue_processor_local_storage
{
public:
  explicit mpsc_ring_queue_processor_local_storage() noexcept(false);
  ~mpsc_ring_queue_processor_local_storage() noexcept;

  mpsc_ring_queue_processor_local_storage(
    const mpsc_ring_queue_processor_local_storage&) = delete;
  mpsc_ring_queue_processor_local_storage& operator=(
    const mpsc_ring_queue_processor_local_storage&) = delete;
  mpsc_ring_queue_processor_local_storage(
    mpsc_ring_queue_processor_local_storage&&) = delete;
  mpsc_ring_queue_processor_local_storage& operator=(
    mpsc_ring_queue_processor_local_storage&&) = delete;

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
  using processor_samples =
    detail::mpsc_ring_queue<sample, CapacityPerProcessor>;

  using processor_id_lookup_thread_local_cache =
    typename detail::processor_id_lookup::thread_local_cache;

  auto take_remembered_thread_names() -> detail::thread_name_set;

  detail::processor_id_lookup processor_id_lookup;
  std::vector<processor_samples> samples_by_processor;

  std::mutex remembered_thread_names_mutex;
  detail::thread_name_set remembered_thread_names;

  // TODO(strager): Only create this thread-local variable if it's actually used
  // by processor_id_lookup.
  inline static detail::
    lazy_thread_local<processor_id_lookup_thread_local_cache, Tag>
      processor_id_cache;
};
}

#include <cxxtrace/mpsc_ring_queue_processor_local_storage_impl.h> // IWYU pragma: export

#endif
