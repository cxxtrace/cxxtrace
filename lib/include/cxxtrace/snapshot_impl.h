#ifndef CXXTRACE_SNAPSHOT_IMPL_H
#define CXXTRACE_SNAPSHOT_IMPL_H

#if !defined(CXXTRACE_SNAPSHOT_H)
#error                                                                         \
  "Include <cxxtrace/snapshot.h> instead of including <cxxtrace/snapshot_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/uninitialized.h>
#include <stdexcept>
#include <vector>

namespace cxxtrace {
namespace detail {
struct event
{
  template<class ClockSample>
  explicit event(event_kind kind,
                 sample<ClockSample> sample,
                 time_point begin_timestamp,
                 time_point end_timestamp) noexcept
    : kind{ kind }
    , category{ sample.category }
    , name{ sample.name }
    , thread_id{ sample.thread_id }
    , begin_timestamp{ begin_timestamp }
    , end_timestamp{ end_timestamp }
  {}

  template<class ClockSample>
  explicit event(event_kind kind,
                 sample<ClockSample> sample,
                 time_point begin_timestamp) noexcept
    : kind{ kind }
    , category{ sample.category }
    , name{ sample.name }
    , thread_id{ sample.thread_id }
    , begin_timestamp{ begin_timestamp }
    , end_timestamp{ uninitialized }
  {}

  event_kind kind;

  czstring category;
  czstring name;
  thread_id thread_id;
  time_point begin_timestamp;
  time_point end_timestamp;
};
}

template<class Storage, class Clock>
auto
take_all_events(Storage& storage, Clock& clock) noexcept(false)
  -> events_snapshot
{
  using detail::event;
  using detail::sample_kind;

  auto events = std::vector<event>{};
  for (const auto& sample : storage.take_all_samples()) {
    switch (sample.kind) {
      case sample_kind::enter_span: {
        auto begin_timestamp = clock.make_time_point(sample.time_point);
        events.emplace_back(
          event{ event_kind::incomplete_span, sample, begin_timestamp });
        break;
      }
      case sample_kind::exit_span: {
        auto end_timestamp = clock.make_time_point(sample.time_point);
        auto incomplete_span_event_it =
          std::find_if(events.crbegin(), events.crend(), [](const event& e) {
            return e.kind == event_kind::incomplete_span;
          });
        assert(incomplete_span_event_it != events.crend());
        auto begin_timestamp = incomplete_span_event_it->begin_timestamp;
        events.erase(std::prev(incomplete_span_event_it.base()));
        events.emplace_back(
          event{ event_kind::span, sample, begin_timestamp, end_timestamp });
        break;
      }
    }
  }
  return events_snapshot{ events };
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
