#ifndef CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H
#define CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H

#include <vector>

namespace cxxtrace {
namespace detail {
struct sample;
}

class unbounded_unsafe_storage
{
public:
  explicit unbounded_unsafe_storage() noexcept;
  ~unbounded_unsafe_storage() noexcept;

  unbounded_unsafe_storage(const unbounded_unsafe_storage&) = delete;
  unbounded_unsafe_storage& operator=(const unbounded_unsafe_storage&) = delete;
  unbounded_unsafe_storage(unbounded_unsafe_storage&&) = delete;
  unbounded_unsafe_storage& operator=(unbounded_unsafe_storage&&) = delete;

  auto clear_all_samples() noexcept -> void;

  auto add_sample(detail::sample) noexcept(false) -> void;
  auto take_all_samples() noexcept(false) -> std::vector<detail::sample>;

private:
  std::vector<detail::sample> samples;
};
}

#endif
