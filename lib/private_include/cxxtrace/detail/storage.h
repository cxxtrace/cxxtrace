#ifndef CXXTRACE_DETAIL_STORAGE_H
#define CXXTRACE_DETAIL_STORAGE_H

#include <cxxtrace/detail/sample.h>
#include <vector>

namespace cxxtrace {
namespace detail {
class storage
{
public:
  static auto add_sample(sample) noexcept(false) -> void;
  static auto clear_all_samples() noexcept -> void;
  static auto copy_all_samples() noexcept(false) -> std::vector<sample>;
};
}
}

#endif
