#ifndef CXXTRACE_DETAIL_THREAD_H
#define CXXTRACE_DETAIL_THREAD_H

#include <cstddef>
#include <cstdint>
#include <cxxtrace/detail/attribute.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <experimental/memory_resource>
#include <experimental/unordered_map>
#include <string>

#define CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION 1

namespace cxxtrace {
namespace detail {
struct thread_name_set
{
  explicit thread_name_set() noexcept;
  explicit thread_name_set(std::experimental::pmr::memory_resource*) noexcept;

  thread_name_set(const thread_name_set&) noexcept(false);
  thread_name_set(thread_name_set&&) noexcept;
  thread_name_set& operator=(const thread_name_set&) noexcept(false);
  thread_name_set& operator=(thread_name_set&&) noexcept(false);

  ~thread_name_set();

  auto name_of_thread_by_id(thread_id id) const noexcept -> czstring;

  auto remember_name_of_thread(thread_id,
                               const char* name,
                               std::size_t name_length) noexcept(false) -> void;

  auto fetch_and_remember_name_of_current_thread() noexcept(false) -> void;
  auto fetch_and_remember_name_of_current_thread(
    thread_id current_thread_id) noexcept(false) -> void;

#if defined(__APPLE__) && defined(__MACH__)
  auto fetch_and_remember_name_of_current_thread_libproc(
    thread_id current_thread_id) noexcept(false) -> void;
  auto fetch_and_remember_name_of_current_thread_mach(
    thread_id current_thread_id) noexcept(false) -> void;
  auto fetch_and_remember_name_of_current_thread_pthread(
    thread_id current_thread_id) noexcept(false) -> void;

  auto fetch_and_remember_thread_name_for_id(thread_id) noexcept(false) -> void;
  auto fetch_and_remember_thread_name_for_id_libproc(thread_id) noexcept(false)
    -> void;
#endif

  std::experimental::pmr::unordered_map<thread_id, std::string> names;
};

using processor_id = std::uint32_t;

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_01h
{
public:
  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_0bh
{
public:
  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__)
class processor_id_lookup_x86_cpuid_1fh
{
public:
  static auto get_current_processor_id() noexcept -> processor_id;
};
#endif

#if defined(__x86_64__) && defined(__APPLE__)
class processor_id_lookup_x86_cpuid_uncached
{
public:
  auto get_current_processor_id() const noexcept -> processor_id;

private:
  CXXTRACE_NO_UNIQUE_ADDRESS processor_id_lookup_x86_cpuid_0bh lookup;
};
#endif

#if defined(__x86_64__) && defined(__APPLE__)
class processor_id_lookup_x86_cpuid_commpage_preempt_cached
{
public:
  explicit processor_id_lookup_x86_cpuid_commpage_preempt_cached() noexcept;

  auto get_current_processor_id() const noexcept -> processor_id;

private:
  struct cache
  {
    // A signal bit indicating that this cache has been initialized.
    //
    // If this bit didn't exist, the first calls to get_current_processor_id on
    // a thread will return 0 if *apple_commpage::sched_gen happened to equal 0.
    // (Returning 0 is possibly wrong.)
    //
    // TODO(strager): Because get_current_processor_id is advisory anyway,
    // should we drop this bit and live with being wrong with a probability of
    // 1-in-4-billion?
    static constexpr auto initialized = std::uint64_t{ 1ULL << 63 };

    processor_id id;
    // Either 0, or *apple_commpage::sched_gen bitwise-or cache::initialized.
    std::uint64_t scheduler_generation_and_initialized;
  };

  auto initialize() noexcept -> void;

  static thread_local cache thread_local_cache;

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

// TODO(strager): Merge with cxxtrace_test::backoff.
class backoff
{
public:
  explicit backoff();

  auto yield(debug_source_location) -> void;
};
}
}

#endif
