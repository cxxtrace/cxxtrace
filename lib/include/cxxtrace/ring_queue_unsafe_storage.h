#ifndef CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H
#define CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
namespace detail {
struct snapshot_sample;
}

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

  auto add_sample(czstring category,
                  czstring name,
                  sample_kind,
                  ClockSample time_point,
                  thread_id) noexcept -> void;
  auto add_sample(czstring category,
                  czstring name,
                  sample_kind,
                  ClockSample time_point) noexcept -> void;
  template<class Clock>
  auto take_all_samples(Clock&) noexcept(false)
    -> std::vector<detail::snapshot_sample>;

private:
  detail::ring_queue<detail::sample<ClockSample>, Capacity> samples;
};
}

#include <cxxtrace/ring_queue_unsafe_storage_impl.h>

#endif
