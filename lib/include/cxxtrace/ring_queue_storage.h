#ifndef CXXTRACE_RING_QUEUE_STORAGE_H
#define CXXTRACE_RING_QUEUE_STORAGE_H

#include <cstddef>
#include <cxxtrace/ring_queue_unsafe_storage.h>
#include <cxxtrace/thread.h>
#include <mutex>

namespace cxxtrace {
class samples_snapshot;
namespace detail {
struct sample_site_local_data;
}

template<std::size_t Capacity, class ClockSample>
class ring_queue_storage
{
public:
  explicit ring_queue_storage() noexcept;
  ~ring_queue_storage() noexcept;

  ring_queue_storage(const ring_queue_storage&) = delete;
  ring_queue_storage& operator=(const ring_queue_storage&) = delete;
  ring_queue_storage(ring_queue_storage&&) = delete;
  ring_queue_storage& operator=(ring_queue_storage&&) = delete;

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
  std::mutex mutex{};
  ring_queue_unsafe_storage<Capacity, ClockSample> storage{};
};
}

#include <cxxtrace/ring_queue_storage_impl.h> // IWYU pragma: export

#endif
