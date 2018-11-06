#ifndef CXXTRACE_SNAPSHOT_H
#define CXXTRACE_SNAPSHOT_H

#include <cstddef>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class event_ref;
class events_snapshot;
class incomplete_span_ref;
class incomplete_spans_snapshot;
class unbounded_storage;
enum class event_kind;

namespace detail {
struct event;
struct sample;
}

template<class Storage>
auto
copy_all_events(Storage&) noexcept(false) -> events_snapshot;

template<class Storage>
auto
copy_incomplete_spans(Storage&) noexcept(false) -> incomplete_spans_snapshot;

class events_snapshot
{
public:
  using size_type = std::size_t;

  events_snapshot(const events_snapshot&) noexcept(false);
  events_snapshot(events_snapshot&&) noexcept;
  events_snapshot& operator=(const events_snapshot&) noexcept(false);
  events_snapshot& operator=(events_snapshot&&) noexcept;
  ~events_snapshot() noexcept;

  auto at(size_type index) noexcept(false) -> event_ref;
  auto size() const noexcept -> size_type;

  template<class Predicate>
  auto get_if(Predicate&&) noexcept(false) -> event_ref;

private:
  explicit events_snapshot(std::vector<detail::event> events) noexcept;

  std::vector<detail::event> events;

  template<class Storage>
  friend auto copy_all_events(Storage&) noexcept(false) -> events_snapshot;
};

enum class event_kind
{
  span,
  incomplete_span,
};

class event_ref
{
public:
  auto category() const noexcept -> czstring;
  auto kind() const noexcept -> event_kind;
  auto name() const noexcept -> czstring;
  auto thread_id() const noexcept -> thread_id;

private:
  explicit event_ref(const detail::event* event) noexcept;

  const detail::event* event{ nullptr };

  friend class events_snapshot;
};

class incomplete_spans_snapshot
{
public:
  using size_type = std::size_t;

  incomplete_spans_snapshot(const incomplete_spans_snapshot&) noexcept(false);
  incomplete_spans_snapshot(incomplete_spans_snapshot&&) noexcept;
  incomplete_spans_snapshot& operator=(
    const incomplete_spans_snapshot&) noexcept(false);
  incomplete_spans_snapshot& operator=(incomplete_spans_snapshot&&) noexcept;
  ~incomplete_spans_snapshot() noexcept;

  auto at(size_type index) noexcept(false) -> incomplete_span_ref;
  auto size() const noexcept -> size_type;

private:
  explicit incomplete_spans_snapshot(
    std::vector<detail::sample> samples) noexcept;

  std::vector<detail::sample> samples;

  template<class Storage>
  friend auto copy_incomplete_spans(Storage&) noexcept(false)
    -> incomplete_spans_snapshot;
};

class incomplete_span_ref
{
public:
  auto category() noexcept -> czstring;
  auto name() noexcept -> czstring;

private:
  explicit incomplete_span_ref(detail::sample* sample) noexcept;

  detail::sample* sample{ nullptr };

  friend class incomplete_spans_snapshot;
};
}

#include <cxxtrace/snapshot_impl.h>

#endif
