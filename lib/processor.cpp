#include <cstdint>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/processor.h>

#if CXXTRACE_HAVE_RSEQ
#include <cxxtrace/detail/rseq.h>
#endif

#if CXXTRACE_HAVE_LIBRSEQ
#include <rseq/rseq.h>
#endif

#if CXXTRACE_HAVE_SYSCTL
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <sys/sysctl.h>
#include <system_error>
#endif

#if CXXTRACE_HAVE_APPLE_COMMPAGE
#include <atomic>
#include <cassert>
#include <cstring>
#include <cxxtrace/detail/apple_commpage.h>
#endif

#if CXXTRACE_HAVE_GET_NPROCS_CONF
#include <sys/sysinfo.h>
#endif

#if CXXTRACE_HAVE_SCHED_GETCPU
#include <cassert>
#include <sched.h>
#endif

namespace {
#if defined(__x86_64__)
auto
get_maximum_supported_basic_cpuid() -> std::uint32_t
{
  std::uint32_t eax;
  asm("cpuid" : "=a"(eax) : "a"(0x00) : "ebx", "ecx", "edx");
  // From Volume 2A:
  // 0H: EAX: Maximum Input Value for Basic CPUID Information.
  return eax;
}
#endif

#if defined(__x86_64__) && CXXTRACE_HAVE_PROCESSOR_ID_IN_X86_TSC_AUX
auto
get_maximum_supported_extended_cpuid() -> std::uint32_t
{
  std::uint32_t eax;
  asm("cpuid" : "=a"(eax) : "a"(0x80000000) : "ebx", "ecx", "edx");
  // From Volume 2A:
  // 0H: EAX: Maximum Input Value for Extended Function CPUID Information.
  return eax;
}
#endif
}

namespace cxxtrace {
namespace detail {
auto
get_maximum_processor_id() noexcept(false) -> processor_id
{
#if CXXTRACE_HAVE_SYSCTL
  // TODO(strager): On macOS, if available, use _COMM_PAGE_LOGICAL_CPUS instead
  // of sysctl.
  auto maximum_processor_count = std::int32_t{};
  auto maximum_processor_count_size =
    std::size_t{ sizeof(maximum_processor_count) };
  auto rc = ::sysctlbyname("hw.logicalcpu_max",
                           &maximum_processor_count,
                           &maximum_processor_count_size,
                           nullptr,
                           0);
  if (rc != 0) {
    throw std::system_error{
      errno,
      std::generic_category(),
      "Failed to get maximum processor ID from hw.logicalcpu_max sysctl"
    };
  }
  if (maximum_processor_count_size != sizeof(maximum_processor_count)) {
    throw std::runtime_error{
      "sysctl(hw.logicalcpu_max) returned data with an unexpected size"
    };
  }
  return maximum_processor_count - 1;
#elif CXXTRACE_HAVE_GET_NPROCS_CONF
  return ::get_nprocs_conf() - 1;
#else
#error "Unknown platform"
#endif
}

#if CXXTRACE_HAVE_SCHED_GETCPU
auto
processor_id_lookup_sched_getcpu::supported() noexcept -> bool
{
  auto rc = ::sched_getcpu();
  return rc != -1;
}

auto
processor_id_lookup_sched_getcpu::get_current_processor_id() noexcept
  -> processor_id
{
  auto processor_id = ::sched_getcpu();
  assert(processor_id != -1);
  return processor_id;
}
#endif

#if defined(__x86_64__)
auto
processor_id_lookup_x86_cpuid_01h::supported() noexcept -> bool
{
  return get_maximum_supported_basic_cpuid() >= 0x01;
}

auto
processor_id_lookup_x86_cpuid_01h::get_current_processor_id() noexcept
  -> processor_id
{
  std::uint32_t ebx;
  asm volatile("cpuid" : "=b"(ebx) : "a"(0x01) : "ecx", "edx");
  // From Volume 2A:
  // 01H: EBX: Bits 31-24: Initial APIC ID.
  return ebx >> 24;
}
#endif

#if defined(__x86_64__)
auto
processor_id_lookup_x86_cpuid_0bh::supported() noexcept -> bool
{
  return get_maximum_supported_basic_cpuid() >= 0x0b;
}

auto
processor_id_lookup_x86_cpuid_0bh::get_current_processor_id() noexcept
  -> processor_id
{
  std::uint32_t edx;
  asm volatile("cpuid" : "=d"(edx) : "a"(0x0b) : "ebx", "ecx");
  // From Volume 2A:
  // 0BH: EDX: Bits 31-00: x2APIC ID the current logical processor.
  return edx;
}
#endif

#if defined(__x86_64__)
auto
processor_id_lookup_x86_cpuid_1fh::supported() noexcept -> bool
{
  return get_maximum_supported_basic_cpuid() >= 0x1f;
}

auto
processor_id_lookup_x86_cpuid_1fh::get_current_processor_id() noexcept
  -> processor_id
{
  std::uint32_t edx;
  asm volatile("cpuid" : "=d"(edx) : "a"(0x1f) : "ebx", "ecx");
  // From Volume 2A:
  // 1FH: EDX: Bits 31-00: x2APIC ID the current logical processor.
  return edx;
}
#endif

#if defined(__x86_64__)
auto
processor_id_lookup_x86_cpuid_uncached::supported() const noexcept -> bool
{
  return this->lookup.supported();
}

auto
processor_id_lookup_x86_cpuid_uncached::get_current_processor_id() const
  noexcept -> processor_id
{
  // FIXME(strager): We should detect what is supported by the CPU.
  // TODO(strager): Use CPUID 1FH if supported, as recommended by Intel.
  return this->lookup.get_current_processor_id();
}
#endif

#if defined(__x86_64__) && CXXTRACE_HAVE_APPLE_COMMPAGE
processor_id_lookup_x86_cpuid_commpage_preempt_cached::
  processor_id_lookup_x86_cpuid_commpage_preempt_cached() noexcept
{
  this->initialize();
}

auto
processor_id_lookup_x86_cpuid_commpage_preempt_cached::initialize() noexcept
  -> void
{
#if CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION
  static constexpr char expected_signature[] = "commpage 64-bit\0";
  static_assert(
    sizeof(expected_signature) - 1 == apple_commpage::signature_size,
    "expected_signature should fit exactly in commpage_signature field");

  // Version 1 introduced _COMM_PAGE_SIGNATURE and _COMM_PAGE_VERSION.
  // Version 7 introduced _COMM_PAGE_SCHED_GEN.
  static constexpr auto minimum_required_commpage_version = std::uint16_t{ 7 };

  if (std::memcmp(apple_commpage::signature,
                  expected_signature,
                  apple_commpage::signature_size) != 0) {
    this->commpage_supported = false;
    return;
  }
  if (*apple_commpage::version < minimum_required_commpage_version) {
    this->commpage_supported = false;
    return;
  }
  this->commpage_supported = true;
#endif
}

auto
processor_id_lookup_x86_cpuid_commpage_preempt_cached::update_cache(
  thread_local_cache& cache,
  scheduler_generation_type scheduler_generation) const noexcept -> void
{
  assert(this->commpage_supported);
  cache.id = this->uncached_lookup.get_current_processor_id();
  cache.scheduler_generation = scheduler_generation;
}

auto
processor_id_lookup_x86_cpuid_commpage_preempt_cached::supported() const
  noexcept -> bool
{
  return this->uncached_lookup.supported();
}

auto
processor_id_lookup_x86_cpuid_commpage_preempt_cached::get_current_processor_id(
  thread_local_cache& cache) const noexcept -> processor_id
{
#if CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION
  if (!this->commpage_supported) {
    return this->uncached_lookup.get_current_processor_id();
  }
#endif

  auto scheduler_generation = scheduler_generation_type{
    apple_commpage::sched_gen->load(std::memory_order_seq_cst)
  };
  if (scheduler_generation == cache.scheduler_generation) {
    // This thread was not rescheduled onto a different processor. Our cached
    // processor ID is valid.
  } else {
    // Either this thread was rescheduled onto a different processor, or another
    // thread on the system was scheduled onto a different processor. Our cached
    // processor ID is possibly invalid.
    this->update_cache(cache, scheduler_generation);
    // NOTE(strager): If *apple_commpage::sched_gen changes again (i.e.
    // disagrees with our scheduler_generation automatic variable), ideally we
    // would query the processor ID again. However, this function is a hint
    // anyway; there's no point in getting the "right" answer when it'll be
    // possibly invalid after returning anyway.
  }
  return cache.id;
}

processor_id_lookup_x86_cpuid_commpage_preempt_cached::thread_local_cache::
  thread_local_cache(
    const processor_id_lookup_x86_cpuid_commpage_preempt_cached&
      lookup) noexcept
{
#if CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION
  if (!lookup.commpage_supported) {
    // get_current_processor_id does not use thread_local_cache.
    return;
  }
#endif

  auto scheduler_generation = scheduler_generation_type{
    apple_commpage::sched_gen->load(std::memory_order_seq_cst)
  };
  lookup.update_cache(*this, scheduler_generation);
}
#endif

#if defined(__x86_64__) && CXXTRACE_HAVE_PROCESSOR_ID_IN_X86_TSC_AUX
auto
processor_id_lookup_x86_rdpid::supported() noexcept -> bool
{
  if (get_maximum_supported_basic_cpuid() < 0x07) {
    return false;
  }
  std::uint32_t ecx;
  asm("cpuid" : "=c"(ecx) : "a"(0x07) : "ebx", "edx");
  // From Volume 2A:
  // 07H: ECX: Bit 22: RDPID and IA32_TSC_AUX are available if 1.
  return ecx & (1 << 22);
}

auto
processor_id_lookup_x86_rdpid::get_current_processor_id() noexcept
  -> processor_id
{
  auto id = std::uint64_t{};
  asm("rdpid %[id]" : [ id ] "=r"(id));
  return id;
}
#endif

#if defined(__x86_64__) && CXXTRACE_HAVE_PROCESSOR_ID_IN_X86_TSC_AUX
auto
processor_id_lookup_x86_rdtscp::supported() noexcept -> bool
{
  if (get_maximum_supported_extended_cpuid() < 0x80000001) {
    return false;
  }
  std::uint32_t edx;
  asm("cpuid" : "=d"(edx) : "a"(0x80000001) : "ebx", "ecx");
  // From Volume 2A:
  // 80000001H: EDX: Bit 27: RDTSCP and IA32_TSC_AUX are available if 1.
  return edx & (1 << 27);
}

auto
processor_id_lookup_x86_rdtscp::get_current_processor_id() noexcept
  -> processor_id
{
  auto ecx = std::uint32_t{};
  asm("rdtscp" : "=c"(ecx) : : "eax", "edx");
  return ecx;
}
#endif

#if CXXTRACE_HAVE_RSEQ
processor_id_lookup_rseq::thread_local_cache::thread_local_cache(
  const processor_id_lookup_rseq& lookup) noexcept
  : rseq_{ registered_rseq::register_current_thread() }
  , fallback_cache_{ lookup.fallback_lookup }
{}

auto
processor_id_lookup_rseq::supported() const noexcept -> bool
{
#if CXXTRACE_HAVE_LIBRSEQ
  if (!::rseq_available()) {
    return false;
  }
#else
  return false;
#endif
  return this->fallback_lookup.supported();
}

auto
processor_id_lookup_rseq::get_current_processor_id(
  thread_local_cache& cache) const noexcept -> processor_id
{
  auto cpu_id = cache.rseq_.read_cpu_id();
  if (cpu_id < 0) {
    return this->fallback_lookup.get_current_processor_id(
      cache.fallback_cache_);
  }
  return cpu_id;
}
#endif
}
}
