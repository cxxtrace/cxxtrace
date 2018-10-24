#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/storage.h>
#include <mutex>

namespace cxxtrace {
namespace detail {
namespace {
std::mutex g_mutex{};
std::vector<sample> g_samples{};
}

auto
storage::add_sample(sample s) noexcept(false) -> void
{
  auto lock = std::unique_lock{ g_mutex };
  g_samples.emplace_back(std::move(s));
}

auto
storage::clear_all_samples() noexcept -> void
{
  auto lock = std::unique_lock{ g_mutex };
  g_samples.clear();
}

auto
storage::copy_all_samples() noexcept(false) -> std::vector<sample>
{
  auto lock = std::unique_lock{ g_mutex };
  return g_samples;
}
}
}
