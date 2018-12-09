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

auto
event_ref::begin_timestamp() const -> time_point
{
  return this->event->begin_timestamp;
}

auto
event_ref::end_timestamp() const -> time_point
{
  return this->event->end_timestamp;
}

event_ref::event_ref(const detail::event* event) noexcept
  : event{ event }
{}
}
