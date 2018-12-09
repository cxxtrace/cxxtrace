#ifndef CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_IMPL_H
#define CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_IMPL_H

#if !defined(CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/unbounded_unsafe_storage.h> instead of including <cxxtrace/unbounded_unsafe_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/vector.h>
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
  detail::sample_kind kind,
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
  detail::sample_kind kind,
  ClockSample time_point) noexcept(false) -> void
{
  this->add_sample(category, name, kind, time_point, get_current_thread_id());
}

template<class ClockSample>
auto
unbounded_unsafe_storage<ClockSample>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample<ClockSample>>
{
  return std::move(this->samples);
}
}

#endif
