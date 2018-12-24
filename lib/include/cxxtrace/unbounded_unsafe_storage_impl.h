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
template<class Nocommit>
unbounded_unsafe_storage<Nocommit>::unbounded_unsafe_storage() noexcept =
  default;

template<class Nocommit>
unbounded_unsafe_storage<Nocommit>::~unbounded_unsafe_storage() noexcept =
  default;

template<class Nocommit>
auto
unbounded_unsafe_storage<Nocommit>::reset() noexcept -> void
{
  detail::reset_vector(this->samples);
}

template<class Nocommit>
auto
unbounded_unsafe_storage<Nocommit>::add_sample(
  czstring category,
  czstring name,
  detail::sample_kind kind,
  thread_id thread_id) noexcept(false) -> void
{
  this->samples.emplace_back(detail::sample{ category, name, kind, thread_id });
}

template<class Nocommit>
auto
unbounded_unsafe_storage<Nocommit>::add_sample(
  czstring category,
  czstring name,
  detail::sample_kind kind) noexcept(false) -> void
{
  this->add_sample(category, name, kind, get_current_thread_id());
}

template<class Nocommit>
auto
unbounded_unsafe_storage<Nocommit>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  return std::move(this->samples);
}
}

#endif
