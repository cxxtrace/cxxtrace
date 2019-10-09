#ifndef CXXTRACE_MPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_IMPL_H
#define CXXTRACE_MPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_IMPL_H

#if !defined(CXXTRACE_MPSC_RING_QUEUE_PROCESSOR_LOCAL_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/mpsc_ring_queue_processor_local_storage.h> instead of including <cxxtrace/mpsc_ring_queue_processor_local_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cstddef>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/queue_sink.h> // IWYU pragma: keep
#include <cxxtrace/detail/real_synchronization.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <new>
#include <utility>
#include <vector>
// IWYU pragma: no_include <cxxtrace/clock.h>

namespace cxxtrace {
template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::mpsc_ring_queue_processor_local_storage() noexcept(false)
  : samples_by_processor{ detail::get_maximum_processor_id() + 1 }
{}

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::~mpsc_ring_queue_processor_local_storage() noexcept = default;

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
auto
mpsc_ring_queue_processor_local_storage<CapacityPerProcessor,
                                        Tag,
                                        ClockSample>::reset() noexcept -> void
{
  for (auto& samples : this->samples_by_processor) {
    samples.reset();
  }
}

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
auto
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::add_sample(detail::sample_site_local_data site,
                           ClockSample time_point,
                           thread_id thread_id) noexcept -> void
{
  using detail::mpsc_ring_queue_push_result;

  auto& processor_id_cache = *this->processor_id_cache.get(
    [this](processor_id_lookup_thread_local_cache* uninitialized_cache) {
      return new (uninitialized_cache)
        processor_id_lookup_thread_local_cache{ this->processor_id_lookup };
    });
  auto backoff = detail::backoff{};
retry:
  auto processor_id =
    this->processor_id_lookup.get_current_processor_id(processor_id_cache);
  auto& samples = this->samples_by_processor[processor_id];
  auto result = samples.try_push(
    1, [&](auto data) noexcept {
      data.set(0, sample{ site, thread_id, time_point });
    });
  switch (result) {
    case mpsc_ring_queue_push_result::not_pushed_due_to_contention:
      backoff.yield(CXXTRACE_HERE);
      goto retry;
    case mpsc_ring_queue_push_result::pushed:
      break;
  }
}

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
auto
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::add_sample(detail::sample_site_local_data site,
                           ClockSample time_point) noexcept -> void
{
  this->add_sample(site, time_point, get_current_thread_id());
}

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
template<class Clock>
auto
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::take_all_samples(Clock& clock) noexcept(false)
  -> samples_snapshot
{
  // TODO(strager): Deduplicate code with
  // spsc_ring_queue_processor_local_storage.

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
  {
    auto guard = std::lock_guard<std::mutex>{ this->pop_samples_mutex };
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
  }

  auto named_threads = std::vector<thread_id>{};
  auto thread_names = this->take_remembered_thread_names();
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

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
auto
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::remember_current_thread_name_for_next_snapshot() -> void
{
  auto guard = std::lock_guard{ this->remembered_thread_names_mutex };
  this->remembered_thread_names.fetch_and_remember_name_of_current_thread();
}

template<std::size_t CapacityPerProcessor, class Tag, class ClockSample>
auto
mpsc_ring_queue_processor_local_storage<
  CapacityPerProcessor,
  Tag,
  ClockSample>::take_remembered_thread_names() -> detail::thread_name_set
{
  auto guard = std::lock_guard{ this->remembered_thread_names_mutex };
  return std::move(this->remembered_thread_names);
}
}

#endif
