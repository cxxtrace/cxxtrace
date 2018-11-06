#ifndef CXXTRACE_RING_QUEUE_STORAGE_H
#define CXXTRACE_RING_QUEUE_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
template<std::size_t Capacity>
class ring_queue_storage
{
public:
  explicit ring_queue_storage() noexcept;
  ~ring_queue_storage() noexcept;

  ring_queue_storage(const ring_queue_storage&) = delete;
  ring_queue_storage& operator=(const ring_queue_storage&) = delete;
  ring_queue_storage(ring_queue_storage&&) = delete;
  ring_queue_storage& operator=(ring_queue_storage&&) = delete;

  auto clear_all_samples() noexcept -> void;

  auto add_sample(detail::sample) noexcept -> void;
  auto take_all_samples() noexcept(false) -> std::vector<detail::sample>;

private:
  std::mutex mutex{};
  detail::ring_queue<detail::sample, Capacity> samples;
};
}

#include <cxxtrace/ring_queue_storage_impl.h>

#endif
