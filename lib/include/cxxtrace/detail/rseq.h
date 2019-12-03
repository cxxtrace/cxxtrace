#ifndef CXXTRACE_DETAIL_RSEQ_H
#define CXXTRACE_DETAIL_RSEQ_H

#include <cstdint>
#include <cxxtrace/detail/have.h>
// IWYU pragma: no_include <linux/rseq.h>

#if CXXTRACE_HAVE_RSEQ
struct rseq; // IWYU pragma: keep
#endif

// @@@ conditional plz
#include <cxxtrace/detail/atomic_ref.h>
#include <linux/rseq.h>

// @@@ namespace xxx_ properly.
// @@@ tweak casts
#define CXXTRACE_BEGIN_PREEMPTABLE(rseq, preempt_label)                                                     \
  { /* @@@ document scope open */                                                                           \
    /* @@@ dedupe section name */                                                                           \
                                                                                                            \
    ::rseq_cs* critical_section_descriptor; \
    asm (".pushsection .data_cxxtrace_rseq, \"a\", @progbits\n"                                                                 \
             ".critical_section_descriptor_%=:\n"                                                           \
             ".long 0\n"                                                                                    \
             ".long 0\n"                                                                                    \
             ".quad %l[begin_ip]\n"                                                                        \
             ".quad %l[post_commit_ip] - %l[begin_ip]\n"                                                          \
             ".quad %l[abort_ip] + 7\n"                                                            \
             ".popsection\n"                                                                                \
             /*@@@ we should be rip-relative plz */ \
             "movq $.critical_section_descriptor_%=, %[critical_section_descriptor]\n"                                                \
             : [critical_section_descriptor] "=r"(critical_section_descriptor)                               \
             : [begin_ip] "i"(&&xxx_begin)                               \
             /*, [post_commit_offset] "X"((std::uintptr_t)&&xxx_end - (std::uintptr_t)&&xxx_begin)*/                               \
             , [post_commit_ip] "i"(&&xxx_end)                               \
             , [abort_ip] "i"(&&xxx_pre_preempted)                               \
             : );                                                                              \
                                                                                                            \
    /*static const auto __attribute__((section(".data_cxxtrace_rseq")))                                     \
      xxx_critical_section_descriptor = ::rseq_cs{                                                          \
      0,                                                                                                    \
      0,                                                                                                    \
      (std::uintptr_t) && xxx_begin,                                                                        \
      (std::uintptr_t) && xxx_end - (std::uintptr_t) && xxx_begin,                                          \
      (std::uintptr_t) && xxx_pre_preempted + 7,                                                            \
    };*/                                                                                                    \
                                                                                                            \
  xxx_begin : {                                                                                             \
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);                                                \
    asm goto("" : : : : xxx_begin, xxx_end, xxx_pre_preempted);                                             \
    /* @@@ assert lock-free */                                                                              \
    ::cxxtrace::detail::atomic_ref<unsigned long long/*@@@*/>{ (rseq).get_rseq()->rseq_cs.ptr }.store(    \
      /*(std::uintptr_t)&xxx_critical_section_descriptor,*/ \
      (std::uintptr_t)critical_section_descriptor, \
      ::std::memory_order_relaxed); \
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);                                                \
  }

#define CXXTRACE_ON_PREEMPT(scheduler, preempt_label)                          \
  xxx_end: \
    goto xxx_end_2; \
  } /* @@@ document close scope */                                             \
  { \
    xxx_pre_preempted: \
      /* @@@ */ \
      asm(".byte 0x0f, 0xb9, 0x3d\n" \
        ".long 0x53053053"); \
    /*xxx_preempted: @@@*/

// @@@ see which of the signal fences and asm gotos are necessary.
#define CXXTRACE_END_PREEMPTABLE(rseq, preempt_label)                          \
  } /* @@@ document close scope */                                             \
  if (false) { \
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);                     \
    asm goto(""                                                                  \
             :                                                                   \
             :                                                                   \
             :                                                                   \
             : xxx_begin,                                                        \
               xxx_end,                                                          \
               xxx_end_2,                                                        \
               xxx_pre_preempted); /* @@@ doesn't do much. delete? */            \
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);                     \
    xxx_end_2:;                                                                    \
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);                     \
    asm goto("" : : : : xxx_begin, xxx_end, xxx_pre_preempted);                  \
    ::std::atomic_signal_fence(::std::memory_order_seq_cst); \
  }

namespace cxxtrace {
namespace detail {
class registered_rseq
{
public:
  using cpu_id_type = std::int32_t;

  static auto register_current_thread() noexcept -> registered_rseq;

  registered_rseq(const registered_rseq&) = delete;
  registered_rseq& operator=(const registered_rseq&) = delete;

  ~registered_rseq();

  [[gnu::always_inline]] auto get() const noexcept -> ::rseq*
  {
    return this->rseq;
  }

  auto read_cpu_id() const noexcept -> cpu_id_type;
  static auto read_cpu_id(const ::rseq&) noexcept -> cpu_id_type;

private:
  explicit registered_rseq(::rseq* rseq) noexcept;

  ::rseq* rseq;
};
}
}

#endif
