#ifndef CXXTRACE_DETAIL_THREAD_H
#define CXXTRACE_DETAIL_THREAD_H

#include <cstddef>
#include <cstdint>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <experimental/memory_resource>
#include <experimental/unordered_map>
#include <string>

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

auto
get_current_processor_id() noexcept -> processor_id;

#if defined(__x86_64__)
auto
get_current_processor_id_x86_cpuid_01h() noexcept -> processor_id;
auto
get_current_processor_id_x86_cpuid_0bh() noexcept -> processor_id;
auto
get_current_processor_id_x86_cpuid_1fh() noexcept -> processor_id;
#endif

#if defined(__x86_64__) && defined(__APPLE__)
auto
get_current_processor_id_x86_cpuid_commpage_preempt_cached() noexcept
  -> processor_id;
#endif
}
}

#endif
