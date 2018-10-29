#ifndef CXXTRACE_SNAPSHOT_IMPL_H
#define CXXTRACE_SNAPSHOT_IMPL_H

#if !defined(CXXTRACE_SNAPSHOT_H)
#error                                                                         \
  "Include <cxxtrace/snapshot.h> instead of including <cxxtrace/snapshot_impl.h> directly."
#endif

#include <stdexcept>

namespace cxxtrace {
template<class Predicate>
auto
events_snapshot::get_if(Predicate&& predicate) noexcept(false) -> event_ref
{
  auto size = this->size();
  for (auto i = size_type{ 0 }; i < size; ++i) {
    auto event = this->at(i);
    if (predicate(static_cast<const event_ref&>(event))) {
      return event;
    }
  }
  throw std::out_of_range{ "No events match predicate" };
}
}

#endif
