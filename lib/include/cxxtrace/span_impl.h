#ifndef CXXTRACE_SPAN_IMPL_H
#define CXXTRACE_SPAN_IMPL_H

#if !defined(CXXTRACE_SPAN_H)
#error                                                                         \
  "Include <cxxtrace/span.h> instead of including <cxxtrace/span_impl.h> directly."
#endif

#include <cxxtrace/detail/sample.h>
#include <cxxtrace/span.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_storage.h>
#include <vector>

namespace cxxtrace {
namespace detail {
template<class Storage, class Clock>
span_guard<Storage, Clock>::~span_guard() noexcept
{
  this->exit();
}

template<class Storage, class Clock>
auto
span_guard<Storage, Clock>::enter(Storage& storage,
                                  Clock& clock,
                                  czstring category,
                                  czstring name) noexcept(false) -> span_guard
{
  auto begin_timestamp = clock.query();
  storage.add_sample({ category, name, sample_kind::enter_span },
                     begin_timestamp);
  return span_guard{ storage, clock, category, name };
}

template<class Storage, class Clock>
span_guard<Storage, Clock>::span_guard(Storage& storage,
                                       Clock& clock,
                                       czstring category,
                                       czstring name) noexcept
  : storage{ storage }
  , clock{ clock }
  , category{ category }
  , name{ name }
{}

template<class Storage, class Clock>
auto
span_guard<Storage, Clock>::exit() noexcept(false) -> void
{
  auto end_timestamp = clock.query();
  this->storage.add_sample(
    { this->category, this->name, sample_kind::exit_span }, end_timestamp);
}
}
}

#endif
