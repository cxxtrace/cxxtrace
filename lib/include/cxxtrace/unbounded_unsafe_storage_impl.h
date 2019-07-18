#ifndef CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/unbounded_unsafe_storage.h> instead of including <cxxtrace/unbounded_unsafe_storage_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/detail/vector.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
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
  czstring category,
  czstring name,
  sample_kind kind,
  ClockSample time_point,
  thread_id thread_id) noexcept(false) -> void
{
  this->samples.emplace_back(
    detail::sample<ClockSample>{ category, name, kind, thread_id, time_point });
}

template<class ClockSample>
auto
unbounded_unsafe_storage<ClockSample>::add_sample(
  czstring category,
  czstring name,
  sample_kind kind,
  ClockSample time_point) noexcept(false) -> void
{
  this->add_sample(category, name, kind, time_point, get_current_thread_id());
}

template<class ClockSample>
template<class Clock>
auto
unbounded_unsafe_storage<ClockSample>::take_all_samples(Clock& clock) noexcept(
  false) -> samples_snapshot
{
  auto samples = std::move(this->samples);

  auto thread_ids = std::vector<thread_id>{};
  for (const auto& sample : samples) {
    auto id = sample.thread_id;
    if (std::find(thread_ids.begin(), thread_ids.end(), id) ==
        thread_ids.end()) {
      thread_ids.emplace_back(id);
    }
  }
  auto thread_names = std::move(this->remembered_thread_names);
  thread_names.fetch_and_remember_thread_names_for_ids(
    thread_ids.data(), &thread_ids.data()[thread_ids.size()]);

  return samples_snapshot{ detail::snapshot_sample::many_from_samples(
                             samples.begin(), samples.end(), clock),
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
