#ifndef CXXTRACE_UNINITIALIZED_H
#define CXXTRACE_UNINITIALIZED_H

namespace cxxtrace {
struct uninitialized_t
{
  explicit constexpr uninitialized_t() noexcept = default;
};

inline constexpr uninitialized_t uninitialized{};
}

#endif
