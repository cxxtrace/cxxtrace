#ifndef CXXTRACE_UNBOUNDED_STORAGE_IMPL_H
#define CXXTRACE_UNBOUNDED_STORAGE_IMPL_H

#if !defined(CXXTRACE_UNBOUNDED_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/unbounded_storage.h> instead of including <cxxtrace/unbounded_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_unsafe_storage_impl.h>
#include <mutex> // IWYU pragma: keep

namespace cxxtrace {
template<class ClockSample>
unbounded_storage<ClockSample>::unbounded_storage() noexcept = default;

template<class ClockSample>
unbounded_storage<ClockSample>::~unbounded_storage() noexcept = default;

template<class ClockSample>
auto
unbounded_storage<ClockSample>::reset() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.reset();
}

template<class ClockSample>
auto
unbounded_storage<ClockSample>::add_sample(detail::sample_site_local_data site,
                                           ClockSample time_point,
                                           thread_id thread_id) noexcept(false)
  -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(site, time_point, thread_id);
}

template<class ClockSample>
auto
unbounded_storage<ClockSample>::add_sample(
  detail::sample_site_local_data site,
  ClockSample time_point) noexcept(false) -> void
{
  this->add_sample(site, time_point, get_current_thread_id());
}

template<class ClockSample>
template<class Clock>
auto
unbounded_storage<ClockSample>::take_all_samples(Clock& clock) noexcept(false)
  -> samples_snapshot
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples(clock);
}

template<class ClockSample>
auto
unbounded_storage<ClockSample>::remember_current_thread_name_for_next_snapshot()
  -> void
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.remember_current_thread_name_for_next_snapshot();
}
}

#endif
