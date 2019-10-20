#ifndef CXXTRACE_TEST_RSEQ_SCHEDULER_SYNCHRONIZATION_MIXIN_H
#define CXXTRACE_TEST_RSEQ_SCHEDULER_SYNCHRONIZATION_MIXIN_H

#include <cxxtrace/detail/processor.h>
#include <cxxtrace/string.h>
#include <setjmp.h>

namespace cxxtrace_test {
template<class Derived, class DebugSourceLocation>
class rseq_scheduler_synchronization_mixin
{
public:
  class rseq_handle
  {
  public:
    auto get_current_processor_id(DebugSourceLocation) noexcept
      -> cxxtrace::detail::processor_id;

    auto end_preemptable(DebugSourceLocation) -> void;
    auto enter_critical_section(cxxtrace::czstring preempt_label,
                                DebugSourceLocation) noexcept -> void;
    auto finish_preempt(DebugSourceLocation) noexcept -> void;
    auto preempt_target_jmp_buf() noexcept -> ::jmp_buf&;
  };

  static auto allow_preempt(DebugSourceLocation) -> void;

  static auto get_rseq() -> rseq_handle;
};
}

#endif
