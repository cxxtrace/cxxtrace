#ifndef CXXTRACE_SNAPSHOT_H
#define CXXTRACE_SNAPSHOT_H

#include <cstddef>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class sample_ref;

enum class event_kind;

class samples_snapshot
{
public:
  using size_type = std::size_t;

  explicit samples_snapshot(std::vector<detail::snapshot_sample>,
                            detail::thread_name_set thread_names) noexcept;

  samples_snapshot(const samples_snapshot&) noexcept(false);
  samples_snapshot(samples_snapshot&&) noexcept;
  samples_snapshot& operator=(const samples_snapshot&) noexcept(false);
  samples_snapshot& operator=(samples_snapshot&&) noexcept(false);
  ~samples_snapshot() noexcept;

  auto at(size_type index) const noexcept(false) -> sample_ref;
  auto size() const noexcept -> size_type;

  auto thread_name(thread_id) const noexcept -> czstring;

  // TODO(strager): Expose an iterator interface instead.
  auto thread_ids() const noexcept(false) -> std::vector<thread_id>;

private:
  std::vector<detail::snapshot_sample> samples;
  detail::thread_name_set thread_names;
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

// FIXME(strager): Should this API exist? The semantics are pretty wonky.
//
// For some configurations, remember_current_thread_name_for_next_snapshot
// synchronously fetches the name of the current thread ("thread A") and changes
// the behavior of the next call to take_all_samples (executing on possibly
// another thread ("thread B")):
//
// * If the thread A has exited, take_all_samples includes the
//   thread name queried by remember_current_thread_name_for_next_snapshot.
// * If the thread A is still alive, take_all_samples behaves as if
//   remember_current_thread_name_for_next_snapshot was not called at all and
//   fetches thread's A newest name.
//
// For other configurations, remember_current_thread_name_for_next_snapshot does
// nothing, and has no effect on the behavior of take_all_samples. These
// configurations automatically remember a thread's name if that thread exits.
template<class Config>
auto
remember_current_thread_name_for_next_snapshot(Config&) -> void;
}

#include <cxxtrace/snapshot_impl.h>

#endif
