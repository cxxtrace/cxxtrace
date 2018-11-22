#ifndef CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_unsafe_storage.h> instead of including <cxxtrace/ring_queue_unsafe_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <utility>
#include <vector>

namespace cxxtrace {
template<std::size_t Capacity>
ring_queue_unsafe_storage<Capacity>::ring_queue_unsafe_storage() noexcept =
  default;

template<std::size_t Capacity>
ring_queue_unsafe_storage<Capacity>::~ring_queue_unsafe_storage() noexcept =
  default;

template<std::size_t Capacity>
auto
ring_queue_unsafe_storage<Capacity>::clear_all_samples() noexcept -> void
{
  this->samples.clear();
}

template<std::size_t Capacity>
auto
ring_queue_unsafe_storage<Capacity>::add_sample(detail::sample s) noexcept
  -> void
{
  this->samples.push(
    1, [&s](auto data) noexcept { data.set(0, std::move(s)); });
}

template<std::size_t Capacity>
auto
ring_queue_unsafe_storage<Capacity>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto samples = std::vector<detail::sample>{};
  this->samples.pop_all_into(samples);
  return samples;
}
}

#endif
