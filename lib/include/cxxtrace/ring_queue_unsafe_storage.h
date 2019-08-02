#ifndef CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H
#define CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t Capacity, class ClockSample>
class ring_queue_unsafe_storage
{
public:
  explicit ring_queue_unsafe_storage() noexcept;
  ~ring_queue_unsafe_storage() noexcept;

  ring_queue_unsafe_storage(const ring_queue_unsafe_storage&) = delete;
  ring_queue_unsafe_storage& operator=(const ring_queue_unsafe_storage&) =
    delete;
  ring_queue_unsafe_storage(ring_queue_unsafe_storage&&) = delete;
  ring_queue_unsafe_storage& operator=(ring_queue_unsafe_storage&&) = delete;

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

  detail::ring_queue<sample, Capacity> samples;
  detail::thread_name_set remembered_thread_names;
};
}

#include <cxxtrace/ring_queue_unsafe_storage_impl.h>

#endif
