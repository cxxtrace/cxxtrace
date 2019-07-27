#ifndef CXXTRACE_SPSC_RING_QUEUE_THREAD_LOCAL_STORAGE_IMPL_H
#define CXXTRACE_SPSC_RING_QUEUE_THREAD_LOCAL_STORAGE_IMPL_H

#if !defined(CXXTRACE_SPSC_RING_QUEUE_THREAD_LOCAL_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/spsc_ring_queue_thread_local_storage.h> instead of including <cxxtrace/spsc_ring_queue_thread_local_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cxxtrace/detail/lazy_thread_local.h>
#include <cxxtrace/detail/queue_sink.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/spsc_ring_queue.h>
#include <cxxtrace/detail/vector.h>
#include <cxxtrace/detail/workarounds.h>
#include <mutex>
#include <utility>
#include <vector>

namespace cxxtrace {
template<std::size_t CapacityPerThread, class Tag, class ClockSample>
struct spsc_ring_queue_thread_local_storage<CapacityPerThread,
                                            Tag,
                                            ClockSample>::thread_local_sample
{
  czstring category;
  czstring name;
  sample_kind kind;
  ClockSample time_point;
};

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
struct spsc_ring_queue_thread_local_storage<CapacityPerThread,
                                            Tag,
                                            ClockSample>::thread_data
{
  explicit thread_data() noexcept(false)
  {
    spsc_ring_queue_thread_local_storage::add_to_thread_list(this);
  }

  ~thread_data() noexcept
  {
    spsc_ring_queue_thread_local_storage::remove_from_thread_list(this);
  }

  thread_data(const thread_data&) = delete;
  thread_data& operator=(const thread_data&) = delete;
  thread_data(thread_data&&) = delete;
  thread_data& operator=(thread_data&&) = delete;

  auto pop_all_into(std::vector<detail::sample<ClockSample>>& output) noexcept(
    false) -> void
  {
    auto thread_id = this->id;
    auto make_sample = [thread_id](const thread_local_sample& sample) noexcept
                         ->detail::sample<ClockSample>
    {
      return detail::sample<ClockSample>{
        sample.category, sample.name, sample.kind, thread_id, sample.time_point
      };
    };
    this->samples.pop_all_into(
      detail::transform_vector_queue_sink{ output, make_sample });
  }

  cxxtrace::thread_id id{ cxxtrace::get_current_thread_id() };
  detail::spsc_ring_queue<thread_local_sample, CapacityPerThread> samples{};
};

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
spsc_ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  reset() noexcept -> void
{
  auto global_lock = std::lock_guard{ global_mutex };
  for (auto* data : thread_list) {
    data->samples.reset();
  }
  detail::reset_vector(disowned_samples);
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
spsc_ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  add_sample(czstring category,
             czstring name,
             sample_kind kind,
             ClockSample time_point) noexcept -> void
{
  auto& thread_data = get_thread_data();
  thread_data.samples.push(1, [&](auto data) noexcept {
    data.set(0, thread_local_sample{ category, name, kind, time_point });
  });
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
spsc_ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  take_all_samples() noexcept(false) -> std::vector<detail::sample<ClockSample>>
{
  auto global_lock = std::lock_guard{ global_mutex };
  auto samples = std::move(disowned_samples);
  for (auto* data : thread_list) {
    data->pop_all_into(samples);
  }
  return samples;
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
spsc_ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
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
spsc_ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  add_to_thread_list(thread_data* data) noexcept(false) -> void
{
  auto global_lock = std::lock_guard{ global_mutex };
  thread_list.emplace_back(data);
}

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
auto
spsc_ring_queue_thread_local_storage<CapacityPerThread, Tag, ClockSample>::
  remove_from_thread_list(thread_data* data) noexcept -> void
{
  auto global_lock = std::lock_guard{ global_mutex };

  auto it = std::remove(thread_list.begin(), thread_list.end(), data);
  assert(it != thread_list.end());
  thread_list.erase(it, thread_list.end());

  data->pop_all_into(disowned_samples);
}
}

#endif
