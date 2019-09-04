#ifndef CXXTRACE_DETAIL_THREAD_H
#define CXXTRACE_DETAIL_THREAD_H

#include <cstddef>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <experimental/memory_resource>
#include <experimental/unordered_map> // IWYU pragma: keep
#include <string>
// IWYU pragma: no_include <unordered_map>

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
  auto allocate_name(thread_id, std::size_t max_name_length) noexcept(false)
    -> std::string&;

  auto fetch_and_remember_name_of_current_thread() noexcept(false) -> void;
  auto fetch_and_remember_name_of_current_thread(
    thread_id current_thread_id) noexcept(false) -> void;
#if CXXTRACE_HAVE_PROC_PIDINFO
  auto fetch_and_remember_name_of_current_thread_libproc(
    thread_id current_thread_id) noexcept(false) -> void;
#endif
#if CXXTRACE_HAVE_MACH_THREAD
  auto fetch_and_remember_name_of_current_thread_mach(
    thread_id current_thread_id) noexcept(false) -> void;
#endif
#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP
  auto fetch_and_remember_name_of_current_thread_pthread(
    thread_id current_thread_id) noexcept(false) -> void;
#endif

  auto fetch_and_remember_thread_name_for_id(thread_id) noexcept(false) -> void;
#if CXXTRACE_HAVE_PROC_PIDINFO
  auto fetch_and_remember_thread_name_for_id_libproc(thread_id) noexcept(false)
    -> void;
#endif
#if CXXTRACE_HAVE_LINUX_PROCFS
  auto fetch_and_remember_thread_name_for_id_procfs(thread_id) noexcept(false)
    -> void;
#endif

  std::experimental::pmr::unordered_map<thread_id, std::string> names;
};

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
