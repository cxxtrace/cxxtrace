#include <cassert>
#include <cxxtrace/config.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/storage.h>
#include <iterator>
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

template auto
copy_all_events<unbounded_storage>(unbounded_storage&) noexcept(false)
  -> events_snapshot;

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

template auto
copy_incomplete_spans<unbounded_storage>(unbounded_storage&) noexcept(false)
  -> incomplete_spans_snapshot;

events_snapshot::events_snapshot(const events_snapshot&) noexcept(false) =
  default;
events_snapshot::events_snapshot(events_snapshot&&) noexcept = default;
events_snapshot&
events_snapshot::operator=(const events_snapshot&) noexcept(false) = default;
events_snapshot&
events_snapshot::operator=(events_snapshot&&) noexcept = default;
events_snapshot::~events_snapshot() noexcept = default;

auto
events_snapshot::at(size_type index) noexcept(false) -> event_ref
{
  return event_ref{ &this->events.at(index) };
}

auto
events_snapshot::size() const noexcept -> size_type
{
  return size_type{ this->events.size() };
}

events_snapshot::events_snapshot(std::vector<detail::event> events) noexcept
  : events{ events }
{}

auto
event_ref::category() const noexcept -> czstring
{
  return this->event->sample.category;
}

auto
event_ref::kind() const noexcept -> event_kind
{
  return this->event->kind;
}

auto
event_ref::name() const noexcept -> czstring
{
  return this->event->sample.name;
}

auto
event_ref::thread_id() const noexcept -> cxxtrace::thread_id
{
  return this->event->sample.thread_id;
}

event_ref::event_ref(const detail::event* event) noexcept
  : event{ event }
{}

incomplete_spans_snapshot::incomplete_spans_snapshot(
  const incomplete_spans_snapshot&) noexcept(false) = default;
incomplete_spans_snapshot::incomplete_spans_snapshot(
  incomplete_spans_snapshot&&) noexcept = default;
incomplete_spans_snapshot&
incomplete_spans_snapshot::operator=(const incomplete_spans_snapshot&) noexcept(
  false) = default;
incomplete_spans_snapshot&
incomplete_spans_snapshot::operator=(incomplete_spans_snapshot&&) noexcept =
  default;
incomplete_spans_snapshot::~incomplete_spans_snapshot() noexcept = default;

auto
incomplete_spans_snapshot::at(size_type index) noexcept(false)
  -> incomplete_span_ref
{
  return incomplete_span_ref{ &this->samples.at(index) };
}

auto
incomplete_spans_snapshot::size() const noexcept -> size_type
{
  return size_type{ this->samples.size() };
}

incomplete_spans_snapshot::incomplete_spans_snapshot(
  std::vector<detail::sample> samples) noexcept
  : samples{ samples }
{}

auto
incomplete_span_ref::category() noexcept -> czstring
{
  return this->sample->category;
}

auto
incomplete_span_ref::name() noexcept -> czstring
{
  return this->sample->name;
}

incomplete_span_ref::incomplete_span_ref(detail::sample* sample) noexcept
  : sample{ sample }
{}
}
