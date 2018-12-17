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
inline unbounded_unsafe_storage::unbounded_unsafe_storage() noexcept = default;

inline unbounded_unsafe_storage::~unbounded_unsafe_storage() noexcept = default;

inline auto
unbounded_unsafe_storage::reset() noexcept -> void
{
  detail::reset_vector(this->samples);
}

inline auto
unbounded_unsafe_storage::add_sample(czstring category,
                                     czstring name,
                                     detail::sample_kind kind,
                                     thread_id thread_id) noexcept(false)
  -> void
{
  this->samples.emplace_back(detail::sample{ category, name, kind, thread_id });
}

inline auto
unbounded_unsafe_storage::add_sample(czstring category,
                                     czstring name,
                                     detail::sample_kind kind) noexcept(false)
  -> void
{
  this->add_sample(category, name, kind, get_current_thread_id());
}

inline auto
unbounded_unsafe_storage::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  return std::move(this->samples);
}
}

#endif
