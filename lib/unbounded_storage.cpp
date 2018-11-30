#include <cxxtrace/detail/sample.h>
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
unbounded_storage::add_sample(detail::sample s) noexcept(false) -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->storage.add_sample(std::move(s));
}

auto
unbounded_storage::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto lock = std::unique_lock{ this->mutex };
  return this->storage.take_all_samples();
}
}
