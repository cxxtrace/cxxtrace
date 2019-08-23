#ifndef CXXTRACE_MPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H
#define CXXTRACE_MPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/mpmc_ring_queue.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t Capacity, class ClockSample>
class mpmc_ring_queue_processor_local_storage
{
public:
  explicit mpmc_ring_queue_processor_local_storage() noexcept(false);
  ~mpmc_ring_queue_processor_local_storage() noexcept;

  mpmc_ring_queue_processor_local_storage(
    const mpmc_ring_queue_processor_local_storage&) = delete;
  mpmc_ring_queue_processor_local_storage& operator=(
    const mpmc_ring_queue_processor_local_storage&) = delete;
  mpmc_ring_queue_processor_local_storage(
    mpmc_ring_queue_processor_local_storage&&) = delete;
  mpmc_ring_queue_processor_local_storage& operator=(
    mpmc_ring_queue_processor_local_storage&&) = delete;

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
  using processor_samples = detail::mpmc_ring_queue<sample, Capacity>;

  detail::processor_id_lookup processor_id_lookup;
  std::vector<processor_samples> samples_by_processor;
  detail::thread_name_set remembered_thread_names;
};
}

#include <cxxtrace/mpmc_ring_queue_processor_local_storage_impl.h>

#endif
