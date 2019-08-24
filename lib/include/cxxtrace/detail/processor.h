#ifndef CXXTRACE_DETAIL_PROCESSOR_H
#define CXXTRACE_DETAIL_PROCESSOR_H

#include <cstdint>
#include <cxxtrace/detail/attribute.h>

#define CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION 1

namespace cxxtrace {
namespace detail {
using processor_id = std::uint32_t;

auto
get_maximum_processor_id() noexcept(false) -> processor_id;

template<class ProcessorIDLookup>
class cacheless_processor_id_lookup
{
public:
  struct thread_local_cache
  {};

  auto get_current_processor_id(thread_local_cache&) const noexcept
    -> processor_id
  {
    return static_cast<const ProcessorIDLookup&>(*this)
      .get_current_processor_id();
  }
};

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_01h
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_01h>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_0bh
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_0bh>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_1fh
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_1fh>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__) && defined(__APPLE__)
class processor_id_lookup_x86_cpuid_uncached
  : public cacheless_processor_id_lookup<processor_id_lookup_x86_cpuid_uncached>
{
public:
  using cacheless_processor_id_lookup::get_current_processor_id;

  auto get_current_processor_id() const noexcept -> processor_id;

private:
  CXXTRACE_NO_UNIQUE_ADDRESS processor_id_lookup_x86_cpuid_0bh lookup;
};
#endif

#if defined(__x86_64__) && defined(__APPLE__)
class processor_id_lookup_x86_cpuid_commpage_preempt_cached
{
public:
  class thread_local_cache
    : public processor_id_lookup_x86_cpuid_uncached::thread_local_cache
  {
  private:
    // A signal bit indicating that this cache has been initialized.
    //
    // If this bit didn't exist, the first calls to get_current_processor_id on
    // a thread will return 0 if *apple_commpage::sched_gen happened to equal 0.
    // (Returning 0 is possibly wrong.)
    //
    // TODO(strager): Because get_current_processor_id is advisory anyway,
    // should we drop this bit and live with being wrong with a probability of
    // 1-in-4-billion?
    // TODO(strager): Write a constructor to do the initialization. We should be
    // stashed in a guarded thread_local anyway.
    static constexpr auto initialized = std::uint64_t{ 1ULL << 63 };

    processor_id id;
    // Either 0, or *apple_commpage::sched_gen bitwise-or cache::initialized.
    std::uint64_t scheduler_generation_and_initialized;

    friend class processor_id_lookup_x86_cpuid_commpage_preempt_cached;
  };

  explicit processor_id_lookup_x86_cpuid_commpage_preempt_cached() noexcept;

  auto get_current_processor_id(thread_local_cache&) const noexcept
    -> processor_id;

private:
  auto initialize() noexcept -> void;

  CXXTRACE_NO_UNIQUE_ADDRESS processor_id_lookup_x86_cpuid_uncached
    uncached_lookup;
#if CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION
  bool commpage_supported;
#endif
};
#endif

#if defined(__x86_64__) && defined(__APPLE__)
using processor_id_lookup =
  processor_id_lookup_x86_cpuid_commpage_preempt_cached;
#else
#error "Unknown platform"
#endif
}
}

#endif
