#ifndef CXXTRACE_SNAPSHOT_H
#define CXXTRACE_SNAPSHOT_H

#include <cstddef>
#include <cxxtrace/clock.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class sample_ref;
class samples_snapshot;
enum class event_kind;

namespace detail {
struct snapshot_sample;
enum class sample_kind;
}

// TODO(strager): Move sample_kind into its own public header.
using sample_kind = detail::sample_kind;

template<class Storage, class Clock>
auto
take_all_samples(Storage&, Clock&) noexcept(false) -> samples_snapshot;

class samples_snapshot
{
public:
  using size_type = std::size_t;

  samples_snapshot(const samples_snapshot&) noexcept(false);
  samples_snapshot(samples_snapshot&&) noexcept;
  samples_snapshot& operator=(const samples_snapshot&) noexcept(false);
  samples_snapshot& operator=(samples_snapshot&&) noexcept;
  ~samples_snapshot() noexcept;

  auto at(size_type index) noexcept(false) -> sample_ref;
  auto size() const noexcept -> size_type;

private:
  explicit samples_snapshot(std::vector<detail::snapshot_sample>) noexcept;

  std::vector<detail::snapshot_sample> samples;

  template<class Storage, class Clock>
  friend auto take_all_samples(Storage&, Clock&) noexcept(false)
    -> samples_snapshot;
};

class sample_ref
{
public:
  auto category() const noexcept -> czstring;
  auto kind() const noexcept -> sample_kind;
  auto name() const noexcept -> czstring;
  auto thread_id() const noexcept -> thread_id;
  auto timestamp() const -> time_point;

private:
  explicit sample_ref(const detail::snapshot_sample*) noexcept;

  const detail::snapshot_sample* sample{ nullptr };

  friend class samples_snapshot;
};
}

#include <cxxtrace/snapshot_impl.h>

#endif
