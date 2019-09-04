#ifndef CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_thread_local_storage.h> instead of including <cxxtrace/ring_queue_thread_local_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>                      // IWYU pragma: keep
#include <cxxtrace/detail/queue_sink.h> // IWYU pragma: keep
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/detail/vector.h>
#include <cxxtrace/detail/workarounds.h> // IWYU pragma: keep
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <utility>
#include <vector>

#if CXXTRACE_WORK_AROUND_SLOW_THREAD_LOCAL_GUARDS
#include <cxxtrace/detail/lazy_thread_local.h>
#endif

namespace cxxtrace {
// NOTE[ring_queue_thread_local_storage lock order]:
//
// Acquire and release mutexes in the following order:
//
// 1. Lock global_mutex
// 2. Lock thread_data::mutex
// 3. Unlock thread_data::mutex
// 4. Unlock global_mutex

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
struct ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  thread_data
{
  explicit thread_data() noexcept(false)
  {
    ring_queue_thread_local_storage::add_to_thread_list(this);
  }

  ~thread_data() noexcept
  {
    ring_queue_thread_local_storage::remove_from_thread_list(this);
  }

  thread_data(const thread_data&) = delete;
  thread_data& operator=(const thread_data&) = delete;
  thread_data(thread_data&&) = delete;
  thread_data& operator=(thread_data&&) = delete;

  auto pop_all_into(std::vector<disowned_sample>& output) noexcept(false)
    -> void
  {
    auto thread_id = this->id;
    auto make_sample = [thread_id](
                         const sample& sample) noexcept->disowned_sample
    {
      return disowned_sample{
        sample.site,
        thread_id,
        sample.time_point,
      };
    };
    this->samples.pop_all_into(
      detail::transform_vector_queue_sink{ output, make_sample });
  }

  template<class Clock>
  auto pop_all_into(std::vector<detail::snapshot_sample>& output,
                    Clock& clock) noexcept(false) -> void
  {
    auto make_sample = [&](
                         const sample& sample) noexcept->detail::snapshot_sample
    {
      return detail::snapshot_sample{ sample, this->id, clock };
    };
    this->samples.pop_all_into(
      detail::transform_vector_queue_sink{ output, make_sample });
  }

  // See NOTE[ring_queue_thread_local_storage lock order].
  std::mutex mutex{};
  cxxtrace::thread_id id{ cxxtrace::get_current_thread_id() };
  detail::ring_queue<sample, CapacityPerThread> samples{};
};

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  reset() noexcept -> void
{
  auto global_lock = std::lock_guard{ global_mutex };
  for (auto* data : thread_list) {
    auto thread_lock = std::lock_guard{ data->mutex };
    data->samples.reset();
  }
  detail::reset_vector(disowned_samples);
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  add_sample(detail::sample_site_local_data site,
             ClockSample time_point) noexcept -> void
{
  auto& thread_data = get_thread_data();
  auto thread_lock = std::lock_guard{ thread_data.mutex };
  thread_data.samples.push(
    1, [&](auto data) noexcept {
      data.set(0, sample{ site, time_point });
    });
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
template<class Clock>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  take_all_samples(Clock& clock) noexcept(false) -> samples_snapshot
{
  auto reclaimed_samples = std::vector<disowned_sample>{};
  auto samples = std::vector<detail::snapshot_sample>{};
  auto thread_names = detail::thread_name_set{};
  auto thread_ids = std::vector<thread_id>{};
  {
    auto global_lock = std::lock_guard{ global_mutex };
    reclaimed_samples = std::move(disowned_samples);
    thread_names = std::move(disowned_thread_names);
    thread_ids.reserve(thread_list.size());
    for (auto* data : thread_list) {
      auto thread_lock = std::lock_guard{ data->mutex };
      data->pop_all_into(samples, clock);
      thread_ids.emplace_back(data->id);
    }
  }
  detail::snapshot_sample::many_from_samples(reclaimed_samples, clock, samples);
  detail::reset_vector(reclaimed_samples);

  for (const auto& thread_id : thread_ids) {
    thread_names.fetch_and_remember_thread_name_for_id(thread_id);
  }

  return samples_snapshot{ std::move(samples), std::move(thread_names) };
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  get_thread_data() -> thread_data&
{
#if CXXTRACE_WORK_AROUND_SLOW_THREAD_LOCAL_GUARDS
  struct tag
  {};
  return *detail::lazy_thread_local<thread_data, tag>::get();
#else
  thread_local auto data = thread_data{};
  auto* data_pointer = &data;
#if CXXTRACE_WORK_AROUND_THREAD_LOCAL_OPTIMIZER
  asm volatile("" : "+r"(data_pointer));
#endif
  return *data_pointer;
#endif
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  add_to_thread_list(thread_data* data) noexcept(false) -> void
{
  auto global_lock = std::lock_guard{ global_mutex };
  thread_list.emplace_back(data);
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  remove_from_thread_list(thread_data* data) noexcept -> void
{
  auto global_lock = std::lock_guard{ global_mutex };

  auto it = std::remove(thread_list.begin(), thread_list.end(), data);
  assert(it != thread_list.end());
  thread_list.erase(it, thread_list.end());

  data->pop_all_into(disowned_samples);
  disowned_thread_names.fetch_and_remember_name_of_current_thread(data->id);
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  remember_current_thread_name_for_next_snapshot() -> void
{
  // Do nothing. remove_from_thread_list will remember this thread's name.
}
}

#endif
