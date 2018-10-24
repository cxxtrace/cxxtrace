#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/storage.h>
#include <cxxtrace/span.h>
#include <cxxtrace/string.h>
#include <vector>

namespace cxxtrace {
namespace detail {
span_guard::~span_guard() noexcept
{
  this->exit();
}

auto
span_guard::enter(czstring category, czstring name) noexcept(false)
  -> span_guard
{
  storage::add_sample(sample{ category, name, sample_kind::enter_span });
  return span_guard{ category, name };
}

span_guard::span_guard(czstring category, czstring name) noexcept
  : category{ category }
  , name{ name }
{}

auto
span_guard::exit() noexcept(false) -> void
{
  storage::add_sample(sample{ category, name, sample_kind::exit_span });
}
}
}
