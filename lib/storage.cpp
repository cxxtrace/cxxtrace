#include <cxxtrace/detail/sample.h>
#include <cxxtrace/storage.h>
#include <mutex>

namespace cxxtrace {
unbounded_storage::unbounded_storage() noexcept = default;
unbounded_storage::~unbounded_storage() noexcept = default;

auto
unbounded_storage::clear_all_samples() noexcept -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->samples.clear();
}

auto
unbounded_storage::add_sample(detail::sample s) noexcept(false) -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->samples.emplace_back(std::move(s));
}

auto
unbounded_storage::copy_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  auto lock = std::unique_lock{ this->mutex };
  return this->samples;
}
}
