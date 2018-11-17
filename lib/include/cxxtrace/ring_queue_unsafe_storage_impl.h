#ifndef CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_unsafe_storage.h> instead of including <cxxtrace/ring_queue_unsafe_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/thread.h>
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
ring_queue_unsafe_storage<Capacity>::reset() noexcept -> void
{
  this->samples.reset();
}

template<std::size_t Capacity>
auto
ring_queue_unsafe_storage<Capacity>::add_sample(czstring category,
                                                czstring name,
                                                detail::sample_kind kind,
                                                thread_id thread_id) noexcept
  -> void
{
  this->samples.push(1, [&](auto data) noexcept {
    data.set(0, detail::sample{ category, name, kind, thread_id });
  });
}

template<std::size_t Capacity>
auto
ring_queue_unsafe_storage<Capacity>::add_sample(
  czstring category,
  czstring name,
  detail::sample_kind kind) noexcept -> void
{
  this->add_sample(category, name, kind, get_current_thread_id());
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
