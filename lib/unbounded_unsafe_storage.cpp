#include <cxxtrace/detail/sample.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
#include <utility>
#include <vector>

namespace cxxtrace {
unbounded_unsafe_storage::unbounded_unsafe_storage() noexcept = default;
unbounded_unsafe_storage::~unbounded_unsafe_storage() noexcept = default;

auto
unbounded_unsafe_storage::clear_all_samples() noexcept -> void
{
  this->samples.clear();
}

auto
unbounded_unsafe_storage::add_sample(detail::sample s) noexcept(false) -> void
{
  this->samples.emplace_back(std::move(s));
}

auto
unbounded_unsafe_storage::take_all_samples() noexcept(false)
  -> std::vector<detail::sample>
{
  return std::move(this->samples);
}
}
