#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/storage.h>

namespace cxxtrace {
namespace detail {
namespace {
std::vector<sample> g_samples{};
}

auto
storage::add_sample(sample s) noexcept(false) -> void
{
  g_samples.emplace_back(std::move(s));
}

auto
storage::clear_all_samples() noexcept -> void
{
  g_samples.clear();
}

auto
storage::copy_all_samples() noexcept(false) -> std::vector<sample>
{
  return g_samples;
}
}
}
