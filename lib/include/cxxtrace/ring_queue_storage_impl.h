#ifndef CXXTRACE_RING_QUEUE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_storage.h> instead of including <cxxtrace/ring_queue_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/ring_queue_unsafe_storage.h>
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
ring_queue_storage<Capacity>::reset() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.reset();
}

template<std::size_t Capacity>
auto
ring_queue_storage<Capacity>::add_sample(czstring category,
                                         czstring name,
                                         detail::sample_kind kind,
                                         thread_id thread_id) noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(category, name, kind, thread_id);
}

template<std::size_t Capacity>
auto
ring_queue_storage<Capacity>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples();
}
}

#endif
