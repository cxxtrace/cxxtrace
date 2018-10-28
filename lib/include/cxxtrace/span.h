#ifndef CXXTRACE_SPAN_H
#define CXXTRACE_SPAN_H

#include <cxxtrace/string.h>
#include <type_traits>

namespace cxxtrace {
#define CXXTRACE_SPAN(category, name)                                          \
  CXXTRACE_SPAN_WITH_CONFIG(get_cxxtrace_config(), category, name)

#define CXXTRACE_SPAN_WITH_CONFIG(config, category, name)                      \
  (::cxxtrace::detail::span_guard<::std::remove_reference_t<decltype(          \
     (config).storage())>>::enter((config).storage(), (category), (name)))

namespace detail {
class storage;

template<class Storage>
class span_guard
{
public:
  span_guard(const span_guard&) = delete;
  span_guard(span_guard&&) = delete;
  span_guard& operator=(const span_guard&) = delete;
  span_guard& operator=(span_guard&&) = delete;

  ~span_guard() noexcept;

  static auto enter(Storage&, czstring category, czstring name) noexcept(false)
    -> span_guard;

private:
  explicit span_guard(Storage&, czstring category, czstring name) noexcept;

  auto exit() noexcept(false) -> void;

  Storage& storage;
  czstring category{ nullptr };
  czstring name{ nullptr };
};

extern template class span_guard<detail::storage>;
}
}

#endif
