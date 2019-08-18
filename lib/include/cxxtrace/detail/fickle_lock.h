#ifndef CXXTRACE_DETAIL_FICKLE_LOCK_H
#define CXXTRACE_DETAIL_FICKLE_LOCK_H

#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/mutex.h>
#include <cxxtrace/thread.h>
#include <utility>

namespace cxxtrace {
namespace detail {
// @nocommit document!
class fickle_lock
{
public:
  template<class Func>
  auto try_with_lock(thread_id current_thread_id,
                     Func&& critical_section,
                     debug_source_location caller) -> bool
  {
    auto we_are_leader = this->leader.load(std::memory_order_relaxed,
                                           CXXTRACE_HERE) == current_thread_id;
    if (we_are_leader) {
      return this->try_with_lock_as_leader(
        current_thread_id, std::forward<Func>(critical_section), caller);
    } else {
      return this->try_with_lock_as_outsider_slow(
        current_thread_id, std::forward<Func>(critical_section), caller);
    }
  }

private:
  template<class Func>
  auto try_with_lock_as_leader([[maybe_unused]] thread_id current_thread_id,
                               Func&& critical_section,
                               debug_source_location caller) -> bool
  {
    // @nocommit bug:
    // Store to lock_held_by_leader might be reordered to after load of
    // new_leader. A thread executing try_take_leadership_and_lock would fail to
    // signal that it wants the lock, but would mistakenly think we haven't
    // taken the lock.
    //
    // See also bug comment in try_take_leadership_and_lock.
    //
    // Current solution is to make all four loads and stores seq_cst, or to
    // introduce a seq_cst fence in the middle of the store-load pairs.
    this->lock_held_by_leader.store(
      true, std::memory_order_release, CXXTRACE_HERE);
//atomic_thread_fence(std::memory_order_seq_cst, CXXTRACE_HERE); // @nocommit
    auto acquired = this->new_leader.load(std::memory_order_seq_cst/*@nocommit acquire?*/,
                                          CXXTRACE_HERE) == current_thread_id;
    // perhaps we can issue a store to new_leader here?
    if (acquired) {
      // @nocommit what about exceptions?
      std::forward<Func>(critical_section)();
      this->lock_held_by_leader.store(
        false, std::memory_order_release, CXXTRACE_HERE);
    }
    return acquired;
  }

  template<class Func>
  auto try_with_lock_as_outsider_slow(thread_id current_thread_id,
                                      Func&& critical_section,
                                      debug_source_location) -> bool;

  auto try_take_leadership_and_lock(thread_id current_thread_id,
                                    debug_source_location) noexcept -> bool;

  atomic<thread_id> leader{ invalid_thread_id };
  //atomic<bool> leader_should_gave_up_leadership{ false };
  //atomic<thread_id> new_leader{ invalid_thread_id };
  //atomic<bool> lock_held_by_leader{ false };
  atomic<int> lock_held_by_leader_xxx{ 0 };
  mutex outsider_mutex;
};
}
}

#include <cxxtrace/detail/fickle_lock_impl.h>

#endif
