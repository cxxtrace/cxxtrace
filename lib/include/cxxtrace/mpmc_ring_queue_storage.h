#ifndef CXXTRACE_MPMC_RING_QUEUE_STORAGE_H
#define CXXTRACE_MPMC_RING_QUEUE_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/mpmc_ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t Capacity, class ClockSample>
class mpmc_ring_queue_storage
{
public:
  explicit mpmc_ring_queue_storage() noexcept;
  ~mpmc_ring_queue_storage() noexcept;

  mpmc_ring_queue_storage(const mpmc_ring_queue_storage&) = delete;
  mpmc_ring_queue_storage& operator=(const mpmc_ring_queue_storage&) = delete;
  mpmc_ring_queue_storage(mpmc_ring_queue_storage&&) = delete;
  mpmc_ring_queue_storage& operator=(mpmc_ring_queue_storage&&) = delete;

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

  detail::mpmc_ring_queue<sample, Capacity> samples;
  detail::thread_name_set remembered_thread_names;
};
}

#include <cxxtrace/mpmc_ring_queue_storage_impl.h> // IWYU pragma: export

#endif
