#ifndef CXXTRACE_SPAN_H
#define CXXTRACE_SPAN_H

#include <cxxtrace/string.h>
#include <type_traits>

namespace cxxtrace {
#define CXXTRACE_SPAN_WITH_CONFIG(config, category, name)                      \
  (::cxxtrace::detail::span_guard<                                             \
    ::std::remove_reference_t<decltype((config).storage())>,                   \
    ::std::remove_reference_t<decltype((config).clock())>>::                   \
     enter((config).storage(), (config).clock(), (category), (name)))

namespace detail {
template<class Storage, class Clock>
class span_guard
{
public:
  span_guard(const span_guard&) = delete;
  span_guard(span_guard&&) = delete;
  span_guard& operator=(const span_guard&) = delete;
  span_guard& operator=(span_guard&&) = delete;

  ~span_guard() noexcept;

  static auto enter(Storage&,
                    Clock&,
                    czstring category,
                    czstring name) noexcept(false) -> span_guard;

private:
  explicit span_guard(Storage&,
                      Clock&,
                      czstring category,
                      czstring name) noexcept;

  auto exit() noexcept(false) -> void;

  Storage& storage;
  Clock& clock;
  czstring category{ nullptr };
  czstring name{ nullptr };
};
}
}

#include <cxxtrace/span_impl.h>

#endif
