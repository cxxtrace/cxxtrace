#include <cstddef>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <string>
#include <type_traits>

#if defined(__APPLE__) && defined(__MACH__)
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <libproc.h>
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/thread_info.h>
#include <pthread.h>
#include <sys/proc_info.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#endif

static_assert(std::is_integral_v<cxxtrace::thread_id>);

namespace cxxtrace {
namespace {
#if defined(__APPLE__) && defined(__MACH__)
auto
get_thread_identifier_info(thread_act_t,
                           thread_identifier_info_data_t* out) noexcept
  -> ::kern_return_t;
auto get_thread_identifier_info_or_terminate(thread_act_t) noexcept
  -> thread_identifier_info;

auto
get_thread_extended_info(thread_act_t,
                         thread_extended_info_data_t* out) noexcept
  -> ::kern_return_t;
auto
get_thread_extended_info_or_terminate(thread_act_t thread_port) noexcept
  -> thread_extended_info;
#endif
}

auto
get_current_thread_id() noexcept -> thread_id
{
#if defined(__APPLE__) && defined(__MACH__)
  return get_current_thread_pthread_thread_id();
#else
#error "Unknown platform"
#endif
}

#if defined(__APPLE__) && defined(__MACH__)
auto
get_current_thread_mach_thread_id() noexcept -> std::uint64_t
{
  auto thread_port = mach_thread_self();
  auto info = get_thread_identifier_info_or_terminate(thread_port);
  return thread_id{ info.thread_id };

  // Intentionally leak thread_port. Holding a reference does not prevent the
  // thread from being cleaned up.
}

auto
get_current_thread_pthread_thread_id() noexcept -> std::uint64_t
{
  auto thread_id = std::uint64_t{};
  auto rc = pthread_threadid_np(nullptr, &thread_id);
  if (rc != 0) {
    errno = rc;
    std::perror("fatal: could not determine thread ID of current thread");
    std::terminate();
  }
  return thread_id;
}
#endif

namespace detail {
thread_name_set::thread_name_set() noexcept = default;

thread_name_set::thread_name_set(
  std::experimental::pmr::memory_resource* allocator) noexcept
  : names{ allocator }
{}

thread_name_set::thread_name_set(const thread_name_set&) noexcept(false) =
  default;
thread_name_set::thread_name_set(thread_name_set&&) noexcept = default;
thread_name_set&
thread_name_set::operator=(const thread_name_set&) noexcept(false) = default;
thread_name_set&
thread_name_set::operator=(thread_name_set&&) noexcept(false) = default;
thread_name_set::~thread_name_set() = default;

#if defined(__APPLE__) && defined(__MACH__)
auto
thread_name_set::fetch_and_remember_thread_names_for_ids(
  const thread_id* begin,
  const thread_id* end) noexcept(false) -> void
{
  return this->fetch_and_remember_thread_names_for_ids_libproc(begin, end);
}

auto
thread_name_set::fetch_and_remember_thread_names_for_ids_libproc(
  const thread_id* begin,
  const thread_id* end) noexcept(false) -> void
{
  for (auto it = begin; it != end; ++it) {
    fetch_and_remember_thread_name_for_id_libproc(*it);
  }
}
#endif

auto
thread_name_set::name_of_thread_by_id(thread_id id) const noexcept -> czstring
{
  auto it = this->names.find(id);
  if (it == this->names.end()) {
    return nullptr;
  }
  return it->second.c_str();
}

auto
thread_name_set::fetch_and_remember_name_of_current_thread() noexcept(false)
  -> void
{
  return this->fetch_and_remember_name_of_current_thread(
    get_current_thread_id());
}

auto
thread_name_set::fetch_and_remember_name_of_current_thread(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());
#if defined(__APPLE__) && defined(__MACH__)
  return this->fetch_and_remember_name_of_current_thread_pthread(
    current_thread_id);
#else
#error "Unknown platform"
#endif
}

auto
thread_name_set::fetch_and_remember_name_of_current_thread_libproc(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());
  this->fetch_and_remember_thread_name_for_id_libproc(current_thread_id);
}

auto
thread_name_set::fetch_and_remember_thread_name_for_id(thread_id id) noexcept(
  false) -> void
{
  this->fetch_and_remember_thread_name_for_id_libproc(id);
}

auto
thread_name_set::fetch_and_remember_thread_name_for_id_libproc(
  thread_id id) noexcept(false) -> void
{
  auto info = ::proc_threadinfo{};
  auto info_size =
    ::proc_pidinfo(::getpid(), PROC_PIDTHREADID64INFO, id, &info, sizeof(info));
  if (info_size == 0) {
    auto error = errno;
    if (error == ESRCH) {
      // The thread is dead, or was never alive. Ignore it.
      // TODO(strager): Should we communicate this fact to the caller?
    } else {
      // FIXME(strager): How should we handle this error? Should we throw an
      // exception?
      std::fprintf(stderr,
                   "error: could not get name of current thread: %s\n",
                   std::strerror(error));
    }
    return;
  }
  if (info_size < int{ sizeof(info) }) {
    // FIXME(strager): How should we handle this error? Should we throw an
    // exception?
    std::fprintf(stderr,
                 "error: could not get name of current thread: mismatch in "
                 "structure sizes\n");
    return;
  }
  auto thread_name_length = ::strnlen(info.pth_name, sizeof(info.pth_name));

  auto [it, inserted] =
    this->names.try_emplace(id, info.pth_name, thread_name_length);
  if (!inserted) {
    it->second.assign(info.pth_name, thread_name_length);
  }
}

auto
thread_name_set::fetch_and_remember_name_of_current_thread_mach(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());

  auto thread_port = ::mach_thread_self();
  auto info = get_thread_extended_info_or_terminate(thread_port);
  auto thread_name_length = ::strnlen(info.pth_name, sizeof(info.pth_name));

  auto [it, inserted] = this->names.try_emplace(
    current_thread_id, info.pth_name, thread_name_length);
  if (!inserted) {
    it->second.assign(info.pth_name, thread_name_length);
  }

  // Intentionally leak thread_port. Holding a reference does not prevent the
  // thread from being cleaned up.
}

auto
thread_name_set::fetch_and_remember_name_of_current_thread_pthread(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());

  auto [it, inserted] =
    this->names.try_emplace(current_thread_id, MAXTHREADNAMESIZE, '\0');
  std::string& thread_name = it->second;
  if (!inserted) {
    thread_name.resize(MAXTHREADNAMESIZE);
  }
  assert(thread_name.size() >= MAXTHREADNAMESIZE);

  auto rc = ::pthread_getname_np(
    ::pthread_self(), thread_name.data(), thread_name.size() + 1);
  assert(rc == 0);
  static_cast<void>(rc);
  assert(thread_name.data()[thread_name.size()] == '\0');
  thread_name.resize(std::strlen(thread_name.c_str()));
}

auto
thread_name_set::remember_name_of_thread(
  thread_id id,
  const char* name,
  std::size_t name_length) noexcept(false) -> void
{
  auto [it, inserted] = this->names.try_emplace(id, name, name_length);
  if (!inserted) {
    assert(it != this->names.end());
    it->second.assign(name, name_length);
  }
}
}

namespace {
#if defined(__APPLE__) && defined(__MACH__)
auto
get_thread_identifier_info(thread_act_t thread_port,
                           thread_identifier_info_data_t* out) noexcept
  -> ::kern_return_t
{
  auto info_count = mach_msg_type_number_t{ THREAD_IDENTIFIER_INFO_COUNT };
  return thread_info(thread_port,
                     THREAD_IDENTIFIER_INFO,
                     reinterpret_cast<thread_info_t>(out),
                     &info_count);
}

auto
get_thread_identifier_info_or_terminate(thread_act_t thread_port) noexcept
  -> thread_identifier_info
{
  auto info = thread_identifier_info_data_t{};
  auto rc = get_thread_identifier_info(thread_port, &info);
  if (rc != KERN_SUCCESS) {
    mach_error("fatal: could not determine thread ID of current thread:", rc);
    std::terminate();
  }
  return info;
}

auto
get_thread_extended_info(thread_act_t thread_port,
                         thread_extended_info_data_t* out) noexcept
  -> ::kern_return_t
{
  auto info_count = ::mach_msg_type_number_t{ THREAD_EXTENDED_INFO_COUNT };
  return ::thread_info(thread_port,
                       THREAD_EXTENDED_INFO,
                       reinterpret_cast<thread_info_t>(out),
                       &info_count);
}

auto
get_thread_extended_info_or_terminate(thread_act_t thread_port) noexcept
  -> thread_extended_info
{
  auto info = ::thread_extended_info_data_t{};
  auto rc = get_thread_extended_info(thread_port, &info);
  if (rc != KERN_SUCCESS) {
    ::mach_error("fatal: could not determine thread name of current thread:",
                 rc);
    std::terminate();
  }
  return info;
}
#endif
}
}
