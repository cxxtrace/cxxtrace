#ifndef CXXTRACE_UNBOUNDED_STORAGE_H
#define CXXTRACE_UNBOUNDED_STORAGE_H

#include <cxxtrace/unbounded_unsafe_storage.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
namespace detail {
struct sample;
}

class unbounded_storage
{
public:
  explicit unbounded_storage() noexcept;
  ~unbounded_storage() noexcept;

  unbounded_storage(const unbounded_storage&) = delete;
  unbounded_storage& operator=(const unbounded_storage&) = delete;
  unbounded_storage(unbounded_storage&&) = delete;
  unbounded_storage& operator=(unbounded_storage&&) = delete;

  auto reset() noexcept -> void;

  auto add_sample(detail::sample) noexcept(false) -> void;
  auto take_all_samples() noexcept(false) -> std::vector<detail::sample>;

private:
  std::mutex mutex{};
  unbounded_unsafe_storage storage{};
};
}

#endif
