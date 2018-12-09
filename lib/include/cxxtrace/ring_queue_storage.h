#ifndef CXXTRACE_RING_QUEUE_STORAGE_H
#define CXXTRACE_RING_QUEUE_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/ring_queue_unsafe_storage.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
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

  auto add_sample(czstring category,
                  czstring name,
                  detail::sample_kind,
                  ClockSample time_point,
                  thread_id) noexcept -> void;
  auto add_sample(czstring category,
                  czstring name,
                  detail::sample_kind,
                  ClockSample time_point) noexcept -> void;
  auto take_all_samples() noexcept(false)
    -> std::vector<detail::sample<ClockSample>>;

private:
  std::mutex mutex{};
  ring_queue_unsafe_storage<Capacity, ClockSample> storage{};
};
}

#include <cxxtrace/ring_queue_storage_impl.h>

#endif
