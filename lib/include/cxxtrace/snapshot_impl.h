#ifndef CXXTRACE_SNAPSHOT_IMPL_H
#define CXXTRACE_SNAPSHOT_IMPL_H

#if !defined(CXXTRACE_SNAPSHOT_H)
#error                                                                         \
  "Include <cxxtrace/snapshot.h> instead of including <cxxtrace/snapshot_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cxxtrace/detail/sample.h>
#include <stdexcept>
#include <vector>

namespace cxxtrace {
namespace detail {
struct event
{
  event_kind kind;
  detail::sample sample;
};
}

template<class Storage>
auto
copy_all_events(Storage& storage) noexcept(false) -> events_snapshot
{
  using detail::event;
  using detail::sample_kind;

  auto events = std::vector<event>{};
  for (const auto& sample : storage.copy_all_samples()) {
    switch (sample.kind) {
      case sample_kind::enter_span:
        events.emplace_back(event{ event_kind::incomplete_span, sample });
        break;
      case sample_kind::exit_span:
        auto incomplete_span_event_it =
          std::find_if(events.crbegin(), events.crend(), [](const event& e) {
            return e.kind == event_kind::incomplete_span;
          });
        assert(incomplete_span_event_it != events.crend());
        events.erase(std::prev(incomplete_span_event_it.base()));
        events.emplace_back(event{ event_kind::span, sample });
        break;
    }
  }
  return events_snapshot{ events };
}

template<class Storage>
auto
copy_incomplete_spans(Storage& storage) noexcept(false)
  -> incomplete_spans_snapshot
{
  using namespace detail;

  auto incomplete_spans = std::vector<detail::sample>{};
  for (const auto& sample : storage.copy_all_samples()) {
    switch (sample.kind) {
      case sample_kind::enter_span:
        incomplete_spans.emplace_back(sample);
        break;
      case sample_kind::exit_span:
        incomplete_spans.pop_back();
        break;
    }
  }
  return incomplete_spans_snapshot{ incomplete_spans };
}

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
