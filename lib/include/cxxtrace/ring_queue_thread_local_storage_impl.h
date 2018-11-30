#ifndef CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_IMPL_H
#define CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_IMPL_H

#if !defined(CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/ring_queue_thread_local_storage.h> instead of including <cxxtrace/ring_queue_thread_local_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/lazy_thread_local.h>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/vector.h>
#include <cxxtrace/detail/workarounds.h>
#include <mutex>
#include <utility>
#include <vector>

namespace cxxtrace {
// NOTE[ring_queue_thread_local_storage lock order]:
//
// Acquire and release mutexes in the following order:
//
// 1. Lock global_mutex
// 2. Lock thread_data::mutex
// 3. Unlock thread_data::mutex
// 4. Unlock global_mutex

template<std::size_t CapacityPerThread, class Tag>
struct ring_queue_thread_local_storage<CapacityPerThread, Tag>::thread_data
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

  // See NOTE[ring_queue_thread_local_storage lock order].
  std::mutex mutex{};
  detail::ring_queue<detail::sample, CapacityPerThread> samples{};
};

template<std::size_t CapacityPerThread, class Tag>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag>::reset() noexcept
  -> void
{
  auto global_lock = std::lock_guard{ global_mutex };
  for (auto* data : thread_list) {
    auto thread_lock = std::lock_guard{ data->mutex };
    data->samples.reset();
  }
  detail::reset_vector(disowned_samples);
}

template<std::size_t CapacityPerThread, class Tag>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag>::add_sample(
  detail::sample s) noexcept -> void
{
  auto& thread_data = get_thread_data();
  auto thread_lock = std::lock_guard{ thread_data.mutex };
  thread_data.samples.push(
    1, [&s](auto data) noexcept { data.set(0, std::move(s)); });
}

template<std::size_t CapacityPerThread, class Tag>
auto
ring_queue_thread_local_storage<CapacityPerThread,
                                Tag>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto global_lock = std::lock_guard{ global_mutex };
  auto samples = std::move(disowned_samples);
  for (auto* data : thread_list) {
    auto thread_lock = std::lock_guard{ data->mutex };
    data->samples.pop_all_into(samples);
  }
  return samples;
}

template<std::size_t CapacityPerThread, class Tag>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag>::get_thread_data()
  -> thread_data&
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

template<std::size_t CapacityPerThread, class Tag>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag>::add_to_thread_list(
  thread_data* data) noexcept(false) -> void
{
  auto global_lock = std::lock_guard{ global_mutex };
  thread_list.emplace_back(data);
}

template<std::size_t CapacityPerThread, class Tag>
auto
ring_queue_thread_local_storage<CapacityPerThread, Tag>::
  remove_from_thread_list(thread_data* data) noexcept -> void
{
  auto global_lock = std::lock_guard{ global_mutex };

  auto it = std::remove(thread_list.begin(), thread_list.end(), data);
  assert(it != thread_list.end());
  thread_list.erase(it, thread_list.end());

  data->samples.pop_all_into(disowned_samples);
}
}

#endif
