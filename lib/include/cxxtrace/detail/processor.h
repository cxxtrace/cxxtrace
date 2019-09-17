#ifndef CXXTRACE_DETAIL_PROCESSOR_H
#define CXXTRACE_DETAIL_PROCESSOR_H

#include <cstdint>
#include <cxxtrace/detail/attribute.h>
#include <cxxtrace/detail/have.h>

#define CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION 1

namespace cxxtrace {
namespace detail {
using processor_id = std::uint32_t;

auto
get_maximum_processor_id() noexcept(false) -> processor_id;

enum class processor_id_namespace
{
#if CXXTRACE_HAVE_SCHED_GETCPU
  linux_getcpu,
#endif
#if defined(__x86_64__)
  initial_apic_id,
#endif
};

template<class ProcessorIDLookup>
class cacheless_processor_id_lookup
{
public:
  struct thread_local_cache
  {
    explicit thread_local_cache(const ProcessorIDLookup&) noexcept {}
  };

  auto get_current_processor_id(thread_local_cache&) const noexcept
    -> processor_id
  {
    return static_cast<const ProcessorIDLookup&>(*this)
      .get_current_processor_id();
  }
};

#if CXXTRACE_HAVE_SCHED_GETCPU
class processor_id_lookup_sched_getcpu
  : public cacheless_processor_id_lookup<processor_id_lookup_sched_getcpu>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static constexpr auto id_namespace = processor_id_namespace::linux_getcpu;

  static auto supported() noexcept -> bool;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_01h
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_01h>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static constexpr auto id_namespace = processor_id_namespace::initial_apic_id;

  static auto supported() noexcept -> bool;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_0bh
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_0bh>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static constexpr auto id_namespace = processor_id_namespace::initial_apic_id;

  static auto supported() noexcept -> bool;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_1fh
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_1fh>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static constexpr auto id_namespace = processor_id_namespace::initial_apic_id;

  static auto supported() noexcept -> bool;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_uncached
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_uncached>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static constexpr auto id_namespace = processor_id_namespace::initial_apic_id;

  auto supported() const noexcept -> bool;

  auto get_current_processor_id() const noexcept -> processor_id;

private:
  CXXTRACE_NO_UNIQUE_ADDRESS processor_id_lookup_x86_cpuid_0bh lookup;
};
#endif

#if defined(__x86_64__) && CXXTRACE_HAVE_APPLE_COMMPAGE
class processor_id_lookup_x86_cpuid_commpage_preempt_cached
{
private:
  using scheduler_generation_type = std::uint64_t;

public:
  class thread_local_cache
  {
  public:
    explicit thread_local_cache(
      const processor_id_lookup_x86_cpuid_commpage_preempt_cached&) noexcept;

  private:
    processor_id id;
    scheduler_generation_type scheduler_generation;

    friend class processor_id_lookup_x86_cpuid_commpage_preempt_cached;
  };

  static constexpr auto id_namespace = processor_id_namespace::initial_apic_id;

  explicit processor_id_lookup_x86_cpuid_commpage_preempt_cached() noexcept;

  auto supported() const noexcept -> bool;

  auto get_current_processor_id(thread_local_cache&) const noexcept
    -> processor_id;

private:
  auto initialize() noexcept -> void;
  auto update_cache(thread_local_cache&, scheduler_generation_type) const
    noexcept -> void;

  CXXTRACE_NO_UNIQUE_ADDRESS processor_id_lookup_x86_cpuid_uncached
    uncached_lookup;
#if CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION
  bool commpage_supported;
#endif

  friend class thread_local_cache;
};
#endif

#if CXXTRACE_HAVE_SCHED_GETCPU
using processor_id_lookup = processor_id_lookup_sched_getcpu;
#elif defined(__x86_64__) && CXXTRACE_HAVE_APPLE_COMMPAGE
using processor_id_lookup =
  processor_id_lookup_x86_cpuid_commpage_preempt_cached;
#elif defined(__x86_64__)
using processor_id_lookup = processor_id_lookup_x86_cpuid_0bh;
#else
#error "Unknown platform"
#endif
}
}

#endif
