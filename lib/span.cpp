#include <cxxtrace/detail/sample.h>
#include <cxxtrace/span.h>
#include <cxxtrace/storage.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
namespace detail {
template<class Storage>
span_guard<Storage>::~span_guard() noexcept
{
  this->exit();
}

template<class Storage>
auto
span_guard<Storage>::enter(Storage& storage,
                           czstring category,
                           czstring name) noexcept(false) -> span_guard
{
  storage.add_sample(sample{ category,
                             name,
                             sample_kind::enter_span,
                             cxxtrace::get_current_thread_id() });
  return span_guard{ storage, category, name };
}

template<class Storage>
span_guard<Storage>::span_guard(Storage& storage,
                                czstring category,
                                czstring name) noexcept
  : storage{ storage }
  , category{ category }
  , name{ name }
{}

template<class Storage>
auto
span_guard<Storage>::exit() noexcept(false) -> void
{
  this->storage.add_sample(sample{ this->category,
                                   this->name,
                                   sample_kind::exit_span,
                                   cxxtrace::get_current_thread_id() });
}

template class span_guard<unbounded_storage>;
}
}
