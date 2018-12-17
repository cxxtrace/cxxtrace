#ifndef CXXTRACE_UNBOUNDED_STORAGE_IMPL_H
#define CXXTRACE_UNBOUNDED_STORAGE_IMPL_H

#if !defined(CXXTRACE_UNBOUNDED_STORAGE_H)
#error                                                                         \
  "Include <cxxtrace/unbounded_storage.h> instead of including <cxxtrace/unbounded_storage_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_storage.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
#include <mutex>
#include <utility>

namespace cxxtrace {
inline unbounded_storage::unbounded_storage() noexcept = default;

inline unbounded_storage::~unbounded_storage() noexcept = default;

inline auto
unbounded_storage::reset() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.reset();
}

inline auto
unbounded_storage::add_sample(czstring category,
                              czstring name,
                              detail::sample_kind kind,
                              thread_id thread_id) noexcept(false) -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(category, name, kind, thread_id);
}

inline auto
unbounded_storage::add_sample(czstring category,
                              czstring name,
                              detail::sample_kind kind) noexcept(false) -> void
{
  this->add_sample(category, name, kind, get_current_thread_id());
}

inline auto
unbounded_storage::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples();
}
}

#endif
