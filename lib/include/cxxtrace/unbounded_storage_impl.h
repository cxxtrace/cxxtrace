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
template<class Nocommit>
unbounded_storage<Nocommit>::unbounded_storage() noexcept = default;

template<class Nocommit>
unbounded_storage<Nocommit>::~unbounded_storage() noexcept = default;

template<class Nocommit>
auto
unbounded_storage<Nocommit>::reset() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.reset();
}

template<class Nocommit>
auto
unbounded_storage<Nocommit>::add_sample(czstring category,
                                        czstring name,
                                        detail::sample_kind kind,
                                        thread_id thread_id) noexcept(false)
  -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(category, name, kind, thread_id);
}

template<class Nocommit>
auto
unbounded_storage<Nocommit>::add_sample(
  czstring category,
  czstring name,
  detail::sample_kind kind) noexcept(false) -> void
{
  this->add_sample(category, name, kind, get_current_thread_id());
}

template<class Nocommit>
auto
unbounded_storage<Nocommit>::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples();
}
}

#endif
