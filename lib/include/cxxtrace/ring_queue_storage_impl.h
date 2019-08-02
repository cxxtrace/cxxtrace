#ifndef CXXTRACE_RING_QUEUE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_storage.h> instead of including <cxxtrace/ring_queue_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/ring_queue_unsafe_storage.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <utility>
#include <vector>

namespace cxxtrace {
template<std::size_t Capacity, class ClockSample>
ring_queue_storage<Capacity, ClockSample>::ring_queue_storage() noexcept =
  default;

template<std::size_t Capacity, class ClockSample>
ring_queue_storage<Capacity, ClockSample>::~ring_queue_storage() noexcept =
  default;

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_storage<Capacity, ClockSample>::reset() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.reset();
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_storage<Capacity, ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point,
  thread_id thread_id) noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(site, time_point, thread_id);
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_storage<Capacity, ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point) noexcept -> void
{
  this->add_sample(site, time_point, get_current_thread_id());
}

template<std::size_t Capacity, class ClockSample>
template<class Clock>
auto
ring_queue_storage<Capacity, ClockSample>::take_all_samples(
  Clock& clock) noexcept(false) -> samples_snapshot
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples(clock);
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_storage<Capacity, ClockSample>::
  remember_current_thread_name_for_next_snapshot() -> void
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.remember_current_thread_name_for_next_snapshot();
}
}

#endif
