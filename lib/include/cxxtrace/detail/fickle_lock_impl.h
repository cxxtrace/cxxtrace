#ifndef CXXTRACE_DETAIL_FICKLE_LOCK_IMPL_H
#define CXXTRACE_DETAIL_FICKLE_LOCK_IMPL_H

#if !defined(CXXTRACE_DETAIL_FICKLE_LOCK_H)
#error                                                                         \
  "Include <cxxtrace/detail/fickle_lock.h> instead of including <cxxtrace/detail/fickle_lock_impl.h> directly."
#endif

#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/fickle_lock.h>
#include <cxxtrace/thread.h>

namespace cxxtrace {
namespace detail {
template<class Func>
// @nocommit this noinline is bad in general. see comment below.
[[gnu::noinline]] auto
fickle_lock::try_with_lock_as_outsider_slow(thread_id current_thread_id,
                                            Func&& critical_section,
                                            debug_source_location caller)
  -> bool
{
  auto guard = lock_guard<mutex>{ this->outsider_mutex, CXXTRACE_HERE };
  auto took_leadership =
    this->try_take_leadership_and_lock(current_thread_id, caller);
  // @nocommit if we unlock immediately after calling
  // try_take_leadership_and_lock, we can hoist the critical section call into
  // our caller. This might result in better code generation, since the
  // post-critical-section code would be identical for the fast path and the
  // slow path.
  if (took_leadership) {
    // @nocommit what about exceptions?
    std::forward<Func>(critical_section)();
    this->lock_held_by_leader.store(
      false, std::memory_order_release, CXXTRACE_HERE);
  }
  return took_leadership;
}

inline auto
fickle_lock::try_take_leadership_and_lock(thread_id current_thread_id,
                                          debug_source_location) noexcept
  -> bool
{
  // @nocommit bug:
  // Store to new_leader might be reordered to after load of
  // lock_heald_by_leader. A thread executing try_with_lock_as_leader would fail
  // to signal that it acquired the lock, and would mistakenly think we haven't
  // told it that we want the lock.
  //
  // See also bug comment in try_with_lock_as_leader.
  this->new_leader.store(
    current_thread_id, std::memory_order_seq_cst/*@nocommit release?*/, CXXTRACE_HERE);
//atomic_thread_fence(std::memory_order_seq_cst, CXXTRACE_HERE); // @nocommit
  if (this->lock_held_by_leader.load(std::memory_order_acquire,
                                     CXXTRACE_HERE)) {
    // The old leader won the race and entered the critical section.
    auto old_leader =
      this->leader.load(std::memory_order_relaxed, CXXTRACE_HERE);
    this->new_leader.store(
      old_leader, std::memory_order_release, CXXTRACE_HERE);
    return false;
  }
  // From this point onward, the old leader cannot enter the critical section.
  // (The old leader can only enter if this->new_leader==this->leader.) We have
  // therefore acquired the lock.
  //
  // (Importantly, we do not want loads or stores after this point to be
  // reordered before this point (i.e. before the acquiring load from
  // this->lock_held_by_leader).)

  this->leader.store(
    current_thread_id, std::memory_order_relaxed, CXXTRACE_HERE);
  this->lock_held_by_leader.store(
    true, std::memory_order_relaxed, CXXTRACE_HERE);
  return true;
}
}
}

#endif
