#ifndef CXXTRACE_MPSC_RING_QUEUE_STORAGE_H
#define CXXTRACE_MPSC_RING_QUEUE_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <mutex>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t Capacity, class ClockSample>
class mpsc_ring_queue_storage
{
public:
  explicit mpsc_ring_queue_storage() noexcept;
  ~mpsc_ring_queue_storage() noexcept;

  mpsc_ring_queue_storage(const mpsc_ring_queue_storage&) = delete;
  mpsc_ring_queue_storage& operator=(const mpsc_ring_queue_storage&) = delete;
  mpsc_ring_queue_storage(mpsc_ring_queue_storage&&) = delete;
  mpsc_ring_queue_storage& operator=(mpsc_ring_queue_storage&&) = delete;

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

  auto take_remembered_thread_names() -> detail::thread_name_set;

  detail::mpsc_ring_queue<sample, Capacity> samples;

  std::mutex pop_samples_mutex;

  std::mutex remembered_thread_names_mutex;
  detail::thread_name_set remembered_thread_names;
};
}

#include <cxxtrace/mpsc_ring_queue_storage_impl.h> // IWYU pragma: export

#endif
