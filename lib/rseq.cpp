#include <cxxtrace/detail/have.h> // IWYU pragma: keep

#if CXXTRACE_HAVE_RSEQ
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cxxtrace/detail/atomic_ref.h>
#include <cxxtrace/detail/rseq.h>
#include <linux/rseq.h>
#include <type_traits>
#endif

#if CXXTRACE_HAVE_LIBRSEQ
#include <cassert>
#include <cstdio>
#include <cxxtrace/detail/workarounds.h>
#include <rseq/rseq.h>
#endif

#if CXXTRACE_HAVE_RSEQ && !CXXTRACE_HAVE_LIBRSEQ
#include <cxxtrace/detail/warning.h>
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_HAVE_RSEQ
registered_rseq::registered_rseq(::rseq* rseq) noexcept
  : rseq{ rseq }
{}

auto
registered_rseq::register_current_thread() noexcept -> registered_rseq
{
#if CXXTRACE_HAVE_LIBRSEQ
  auto* rseq = &::__rseq_abi;
#if CXXTRACE_WORK_AROUND_THREAD_LOCAL_OPTIMIZER
  asm volatile("" : "+r"(rseq));
#endif
  auto rc = ::rseq_register_current_thread();
  if (rc == -1) {
    assert(read_cpu_id(*rseq) < 0);
  }
  return registered_rseq{ rseq };
#else
  // TODO(strager): Implement registration using glibc's primitives, if
  // available.
  // TODO(strager): When an ABI is agreed upon, implement registration using
  // syscalls directly.
  CXXTRACE_WARNING_PUSH
  CXXTRACE_WARNING_IGNORE_GCC("-Wmissing-field-initializers")
  static auto fake_rseq = ::rseq{
    .cpu_id = static_cast<std::uint32_t>(RSEQ_CPU_ID_UNINITIALIZED),
  };
  CXXTRACE_WARNING_POP
  return registered_rseq{ &fake_rseq };
#endif
}

registered_rseq::~registered_rseq()
{
  if (this->rseq) {
#if CXXTRACE_HAVE_LIBRSEQ
    auto rc = ::rseq_unregister_current_thread();
    if (rc == -1) {
      std::fprintf(stderr, "warning: rseq_unregister_current_thread failed\n");
      // Ignore the error. At the time of writing,
      // ::rseq_unregister_current_thread only fails if the ref count is already
      // zero, which is a programmer error.
    }
#endif
  }
}

auto
registered_rseq::read_cpu_id() const noexcept -> cpu_id_type
{
  return this->read_cpu_id(*this->rseq);
}

auto
registered_rseq::read_cpu_id(const ::rseq& rseq) noexcept -> cpu_id_type
{
  using signed_real_cpu_id_type = std::make_signed_t<decltype(rseq.cpu_id)>;
  static_assert(std::is_same_v<cpu_id_type, signed_real_cpu_id_type>);

  auto cpu_id = atomic_ref{ const_cast<::rseq&>(rseq).cpu_id }.load(
    std::memory_order_relaxed);
  auto signed_cpu_id = signed_real_cpu_id_type{};
  std::memcpy(&signed_cpu_id, &cpu_id, sizeof(cpu_id));
  return signed_cpu_id;
}
#endif
}
}
