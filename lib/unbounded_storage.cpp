#include <cxxtrace/detail/sample.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_storage.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
#include <mutex>
#include <utility>

namespace cxxtrace {
unbounded_storage::unbounded_storage() noexcept = default;
unbounded_storage::~unbounded_storage() noexcept = default;

auto
unbounded_storage::reset() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.reset();
}

auto
unbounded_storage::add_sample(czstring category,
                              czstring name,
                              detail::sample_kind kind,
                              thread_id thread_id) noexcept(false) -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(category, name, kind, thread_id);
}

auto
unbounded_storage::add_sample(czstring category,
                              czstring name,
                              detail::sample_kind kind) noexcept(false) -> void
{
  this->add_sample(category, name, kind, get_current_thread_id());
}

auto
unbounded_storage::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples();
}
}
