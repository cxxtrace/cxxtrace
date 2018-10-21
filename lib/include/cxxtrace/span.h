#ifndef CXXTRACE_SPAN_H
#define CXXTRACE_SPAN_H

#include <cxxtrace/string.h>

namespace cxxtrace {
#define CXXTRACE_SPAN(category, name)                                          \
  (::cxxtrace::detail::span_guard::enter((category), (name)))

namespace detail {
class span_guard
{
public:
  span_guard(const span_guard&) = delete;
  span_guard(span_guard&&) = delete;
  span_guard& operator=(const span_guard&) = delete;
  span_guard& operator=(span_guard&&) = delete;

  ~span_guard() noexcept;

  static auto enter(czstring category, czstring name) noexcept(false)
    -> span_guard;

private:
  explicit span_guard(czstring category, czstring name) noexcept;

  auto exit() noexcept(false) -> void;

  czstring category{ nullptr };
  czstring name{ nullptr };
};
}
}

#endif
