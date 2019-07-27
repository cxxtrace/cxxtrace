#ifndef CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_unsafe_storage.h> instead of including <cxxtrace/ring_queue_unsafe_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <utility>
#include <vector>

namespace cxxtrace {
template<std::size_t Capacity, class ClockSample>
ring_queue_unsafe_storage<Capacity,
                          ClockSample>::ring_queue_unsafe_storage() noexcept =
  default;

template<std::size_t Capacity, class ClockSample>
ring_queue_unsafe_storage<Capacity,
                          ClockSample>::~ring_queue_unsafe_storage() noexcept =
  default;

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::reset() noexcept -> void
{
  this->samples.reset();
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::add_sample(
  czstring category,
  czstring name,
  sample_kind kind,
  ClockSample time_point,
  thread_id thread_id) noexcept -> void
{
  this->samples.push(1, [&](auto data) noexcept {
    data.set(0,
             detail::sample<ClockSample>{
               category, name, kind, thread_id, time_point });
  });
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::add_sample(
  czstring category,
  czstring name,
  sample_kind kind,
  ClockSample time_point) noexcept -> void
{
  this->add_sample(category, name, kind, time_point, get_current_thread_id());
}

template<std::size_t Capacity, class ClockSample>
template<class Clock>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::take_all_samples(
  Clock& clock) noexcept(false) -> samples_snapshot
{
  static_assert(std::is_same_v<typename Clock::sample, ClockSample>);

  auto samples = std::vector<detail::sample<ClockSample>>{};
  // TODO(strager): Convert to detail::snapshot_sample directly in pop_all_into
  // to avoid an intermediate std::vector.
  this->samples.pop_all_into(samples);
  return samples_snapshot{ detail::snapshot_sample::many_from_samples(
    samples.begin(), samples.end(), clock) };
}
}

#endif
