#ifndef CXXTRACE_MPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_IMPL_H
#define CXXTRACE_MPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_IMPL_H

#if !defined(CXXTRACE_MPMC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/mpmc_ring_queue_processor_local_storage.h> instead of including <cxxtrace/mpmc_ring_queue_processor_local_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/queue_sink.h>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <utility>
#include <vector>

namespace cxxtrace {
template<std::size_t Capacity, class ClockSample>
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::
  mpmc_ring_queue_processor_local_storage() noexcept(false)
  : samples_by_processor{ detail::get_maximum_processor_id() + 1 }
{}

template<std::size_t Capacity, class ClockSample>
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::
  ~mpmc_ring_queue_processor_local_storage() noexcept = default;

template<std::size_t Capacity, class ClockSample>
auto
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::reset() noexcept
  -> void
{
  for (auto& samples : this->samples_by_processor) {
    samples.reset();
  }
}

template<std::size_t Capacity, class ClockSample>
auto
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point,
  thread_id thread_id) noexcept -> void
{
  using detail::mpmc_ring_queue_push_result;

  auto backoff = detail::backoff{};
retry:
  auto processor_id = this->processor_id_lookup.get_current_processor_id(
    this->processor_id_cache);
  auto& samples = this->samples_by_processor[processor_id];
  auto result = samples.try_push(1, [&](auto data) noexcept {
    data.set(0, sample{ site, thread_id, time_point });
  });
  switch (result) {
    case mpmc_ring_queue_push_result::not_pushed_due_to_contention:
      backoff.yield(CXXTRACE_HERE);
      goto retry;
    case mpmc_ring_queue_push_result::pushed:
      break;
  }
}

template<std::size_t Capacity, class ClockSample>
auto
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point) noexcept -> void
{
  this->add_sample(site, time_point, get_current_thread_id());
}

template<std::size_t Capacity, class ClockSample>
template<class Clock>
auto
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::
  take_all_samples(Clock& clock) noexcept(false) -> samples_snapshot
{
  static_assert(std::is_same_v<typename Clock::sample, ClockSample>);

  auto samples = std::vector<detail::snapshot_sample>{};
  auto make_sample = [&](const sample& sample) noexcept->detail::snapshot_sample
  {
    return detail::snapshot_sample{ sample, clock };
  };
  auto snapshot_sample_less_by_clock =
    [](const detail::snapshot_sample& x,
       const detail::snapshot_sample& y) noexcept->bool
  {
    return x.timestamp < y.timestamp;
  };
  // TODO(strager): Avoid excessive copying caused by vector resizes and
  // repeated calls to inplace_merge.
  for (auto& processor_samples : this->samples_by_processor) {
    auto size_before = samples.size();
    processor_samples.pop_all_into(
      detail::transform_vector_queue_sink{ samples, make_sample });
    std::inplace_merge(samples.begin(),
                       samples.begin() + size_before,
                       samples.end(),
                       snapshot_sample_less_by_clock);
  }

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
mpmc_ring_queue_processor_local_storage<Capacity, ClockSample>::
  remember_current_thread_name_for_next_snapshot() -> void
{
  this->remembered_thread_names.fetch_and_remember_name_of_current_thread();
}
}

#endif
