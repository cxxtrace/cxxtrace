#include "cxxtrace_concurrency_test.h"
#include "rseq_scheduler.h"
#include "rseq_scheduler_synchronization_mixin.h"

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include "concurrency_stress_synchronization.h"
#endif

#if CXXTRACE_ENABLE_RELACY
#include "relacy_synchronization.h"
#include <relacy/base.hpp>
#include <relacy/context_base.hpp>
#endif

namespace cxxtrace_test {
namespace {
template<class Sync>
auto
get_rseq_scheduler() -> rseq_scheduler<Sync>&
{
  auto* scheduler = rseq_scheduler<Sync>::get();
  CXXTRACE_ASSERT(scheduler);
  return *scheduler;
}
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::
  rseq_handle::get_current_processor_id(DebugSourceLocation caller) noexcept
  -> cxxtrace::detail::processor_id
{
  return get_rseq_scheduler<Derived>().get_current_processor_id(caller);
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::
  rseq_handle::end_preemptable(DebugSourceLocation caller) -> void
{
  get_rseq_scheduler<Derived>().end_preemptable(caller);
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::
  rseq_handle::enter_critical_section(cxxtrace::czstring preempt_label,
                                      DebugSourceLocation caller) noexcept
  -> void
{
  get_rseq_scheduler<Derived>().enter_critical_section(preempt_label, caller);
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::
  rseq_handle::finish_preempt(DebugSourceLocation caller) noexcept -> void
{
  get_rseq_scheduler<Derived>().finish_preempt(caller);
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::
  rseq_handle::preempt_target_jmp_buf() noexcept -> ::jmp_buf&
{
  return get_rseq_scheduler<Derived>().preempt_target_jmp_buf();
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::
  allow_preempt(DebugSourceLocation caller) -> void
{
  auto* scheduler = rseq_scheduler<Derived>::get();
  if (scheduler) {
    scheduler->allow_preempt(caller);
  }
}

template<class Derived, class DebugSourceLocation>
auto
rseq_scheduler_synchronization_mixin<Derived, DebugSourceLocation>::get_rseq()
  -> rseq_handle
{
  return rseq_handle{};
}

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
template class rseq_scheduler_synchronization_mixin<
  concurrency_stress_synchronization,
  cxxtrace::detail::real_synchronization::debug_source_location>;
#endif

#if CXXTRACE_ENABLE_RELACY
template class rseq_scheduler_synchronization_mixin<relacy_synchronization,
                                                    rl::debug_info>;
#endif
}
