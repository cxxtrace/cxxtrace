#ifndef CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_unsafe_storage.h> instead of including <cxxtrace/ring_queue_unsafe_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cstddef>
#include <cxxtrace/detail/queue_sink.h> // IWYU pragma: keep
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
  detail::sample_site_local_data site,
  ClockSample time_point,
  thread_id thread_id) noexcept -> void
{
  this->samples.push(1, [&](auto data) noexcept {
    data.set(0, sample{ site, thread_id, time_point });
  });
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point) noexcept -> void
{
  this->add_sample(site, time_point, get_current_thread_id());
}

template<std::size_t Capacity, class ClockSample>
template<class Clock>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::take_all_samples(
  Clock& clock) noexcept(false) -> samples_snapshot
{
  static_assert(std::is_same_v<typename Clock::sample, ClockSample>);

  auto samples = std::vector<detail::snapshot_sample>{};
  auto make_sample = [&](const sample& sample) noexcept->detail::snapshot_sample
  {
    return detail::snapshot_sample{ sample, clock };
  };
  this->samples.pop_all_into(
    detail::transform_vector_queue_sink{ samples, make_sample });

  auto named_threads = std::vector<thread_id>{};
  auto thread_names = std::move(this->remembered_thread_names);
  for (const auto& sample : samples) {
    auto id = sample.thread_id;
    if (std::find(named_threads.begin(), named_threads.end(), id) ==
        named_threads.end()) {
      named_threads.emplace_back(id);
      thread_names.fetch_and_remember_thread_name_for_id(id);
    }
  }

  return samples_snapshot{ std::move(samples), std::move(thread_names) };
}

template<std::size_t Capacity, class ClockSample>
auto
ring_queue_unsafe_storage<Capacity, ClockSample>::
  remember_current_thread_name_for_next_snapshot() -> void
{
  this->remembered_thread_names.fetch_and_remember_name_of_current_thread();
}
}

#endif
