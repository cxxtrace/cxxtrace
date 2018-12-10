#include <cassert>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/snapshot.h>
#include <vector>

namespace cxxtrace {
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
  return this->event->category;
}

auto
event_ref::kind() const noexcept -> event_kind
{
  return this->event->kind;
}

auto
event_ref::name() const noexcept -> czstring
{
  return this->event->name;
}

auto
event_ref::thread_id() const noexcept -> cxxtrace::thread_id
{
  return this->event->thread_id;
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
