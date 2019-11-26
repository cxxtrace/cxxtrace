#ifndef CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/unbounded_unsafe_storage.h> instead of including <cxxtrace/unbounded_unsafe_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/vector.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <utility>
#include <vector>

namespace cxxtrace {
template<class ClockSample>
unbounded_unsafe_storage<ClockSample>::unbounded_unsafe_storage() noexcept =
  default;

template<class ClockSample>
unbounded_unsafe_storage<ClockSample>::~unbounded_unsafe_storage() noexcept =
  default;

template<class ClockSample>
auto
unbounded_unsafe_storage<ClockSample>::reset() noexcept -> void
{
  detail::reset_vector(this->samples);
}

template<class ClockSample>
auto
unbounded_unsafe_storage<ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point,
  thread_id thread_id) noexcept(false) -> void
{
  this->samples.emplace_back(sample{ site, thread_id, time_point });
}

template<class ClockSample>
auto
unbounded_unsafe_storage<ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point) noexcept(false) -> void
{
  this->add_sample(site, time_point, get_current_thread_id());
}

template<class ClockSample>
template<class Clock>
auto
unbounded_unsafe_storage<ClockSample>::take_all_samples(Clock& clock) noexcept(
  false) -> samples_snapshot
{
  auto samples = std::vector<sample>{};
  swap(samples, this->samples);

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

  return samples_snapshot{ detail::snapshot_sample::many_from_samples(samples,
                                                                      clock),
                           std::move(thread_names) };
}

template<class ClockSample>
auto
unbounded_unsafe_storage<
  ClockSample>::remember_current_thread_name_for_next_snapshot() -> void
{
  this->remembered_thread_names.fetch_and_remember_name_of_current_thread();
}
}

#endif
