#include <cerrno>
#include <cstddef>
#include <cxxtrace/detail/processor.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cxxtrace/detail/apple_commpage.h>
#include <mach/vm_types.h>
#include <stdexcept>
#include <sys/sysctl.h>
#include <system_error>
#endif

namespace cxxtrace {
namespace detail {
auto
get_maximum_processor_id() noexcept(false) -> processor_id
{
#if defined(__APPLE__) && defined(__MACH__)
  // TODO(strager): If available, use _COMM_PAGE_LOGICAL_CPUS instead of sysctl.
  auto maximum_processor_count = ::integer_t{};
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
#else
#error "Unknown platform"
#endif
}

#if defined(__x86_64__)
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
processor_id_lookup_x86_cpuid_uncached::get_current_processor_id() const
  noexcept -> processor_id
{
  // FIXME(strager): We should detect what is supported by the CPU.
  // TODO(strager): Use CPUID 1FH if supported, as recommended by Intel.
  return this->lookup.get_current_processor_id();
}
#endif

#if defined(__x86_64__) && defined(__APPLE__)
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
}
}
