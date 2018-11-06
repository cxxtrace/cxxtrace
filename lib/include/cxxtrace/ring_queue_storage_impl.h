#ifndef CXXTRACE_RING_QUEUE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_storage.h> instead of including <cxxtrace/ring_queue_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <mutex>
#include <utility>
#include <vector>

namespace cxxtrace {
template<std::size_t Capacity>
ring_queue_storage<Capacity>::ring_queue_storage() noexcept = default;

template<std::size_t Capacity>
ring_queue_storage<Capacity>::~ring_queue_storage() noexcept = default;

template<std::size_t Capacity>
auto
ring_queue_storage<Capacity>::clear_all_samples() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->samples.clear();
}

template<std::size_t Capacity>
auto
ring_queue_storage<Capacity>::add_sample(detail::sample s) noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->samples.push(1, [&s](auto data) noexcept { data[0] = std::move(s); });
}

template<std::size_t Capacity>
auto
ring_queue_storage<Capacity>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto samples = std::vector<detail::sample>{};
  auto lock = std::unique_lock{ this->mutex };
  this->samples.pop_all_into(samples);
  return samples;
}
}

#endif
