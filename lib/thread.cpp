#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <exception>
#include <experimental/memory_resource>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#if CXXTRACE_HAVE_PROC_PIDINFO && !CXXTRACE_HAVE_GETPID
#error "proc_pidinfo should imply getpid"
#endif

#if CXXTRACE_HAVE_LINUX_PROCFS && !CXXTRACE_HAVE_POSIX_FD
#error "Linux procfs should imply POSIX file descriptors"
#endif

#if CXXTRACE_HAVE_MACH_THREAD
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/thread_act.h>
#include <mach/thread_info.h>
#endif

#if CXXTRACE_HAVE_MACH_THREAD || CXXTRACE_HAVE_PTHREAD_THREADID_NP
#include <cstdint>
#endif

#if CXXTRACE_HAVE_PROC_PIDINFO && CXXTRACE_HAVE_GETPID
#include <libproc.h>
#include <unistd.h>
#endif

#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP || CXXTRACE_HAVE_PTHREAD_THREADID_NP
#include <pthread.h>
#endif

#if CXXTRACE_HAVE_PTHREAD_THREADID_NP
#include <sys/proc_info.h>
#endif

#if CXXTRACE_HAVE_LINUX_PROCFS
#include <array>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/detail/string.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if CXXTRACE_HAVE_SYSCALL_GETTID
#include <sys/syscall.h>
#endif

#if defined(__linux__) && !defined(TASK_COMM_LEN)
// See:
// https://github.com/torvalds/linux/blob/eea173097dfbb44855e3cf03c09eb5a665c20438/include/linux/sched.h#L211
#define TASK_COMM_LEN 16
#endif

static_assert(std::is_integral_v<cxxtrace::thread_id>);

namespace cxxtrace {
namespace {
constexpr auto max_thread_name_length =
#if defined(__APPLE__) && defined(__MACH__)
  MAXTHREADNAMESIZE
#elif defined(__linux__)
  TASK_COMM_LEN
#else
#error "Unknown platform"
#endif
  ;

#if CXXTRACE_HAVE_MACH_THREAD
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

class mach_port
{
public:
  explicit mach_port(::mach_port_t port) noexcept
    : port{ port }
  {}

  mach_port(const mach_port&) noexcept = delete;
  mach_port& operator=(const mach_port&) noexcept = delete;

  mach_port(mach_port&& other) noexcept
    : port{ std::exchange(other.port, MACH_PORT_NULL) }
  {}

  mach_port& operator=(mach_port&&) noexcept = delete;

  ~mach_port() { this->release(); }

  auto release() noexcept -> void
  {
    if (this->port == MACH_PORT_NULL) {
      return;
    }
    auto rc = ::mach_port_deallocate(::mach_task_self(), this->port);
    if (rc != KERN_SUCCESS) {
      ::mach_error("fatal: could not deallocate mach port", rc);
      std::terminate();
    }
    this->port = MACH_PORT_NULL;
  }

  ::mach_port_name_t port{ MACH_PORT_NULL };
};
#endif
}

auto
get_current_thread_id() noexcept -> thread_id
{
#if CXXTRACE_HAVE_PTHREAD_THREADID_NP
  return get_current_thread_pthread_thread_id();
#elif CXXTRACE_HAVE_SYSCALL_GETTID
  return get_current_thread_id_gettid();
#else
#error "Unknown platform"
#endif
}

#if CXXTRACE_HAVE_MACH_THREAD
auto
get_current_thread_mach_thread_id() noexcept -> std::uint64_t
{
  auto thread_port = mach_port{ ::mach_thread_self() };
  auto info = get_thread_identifier_info_or_terminate(thread_port.port);
  return thread_id{ info.thread_id };
}
#endif

#if CXXTRACE_HAVE_PTHREAD_THREADID_NP
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

#if CXXTRACE_HAVE_SYSCALL_GETTID
auto
get_current_thread_id_gettid() noexcept -> thread_id
{
  auto thread_id = ::syscall(SYS_gettid);
  if (thread_id < 0) {
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
#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP
  this->fetch_and_remember_name_of_current_thread_pthread(current_thread_id);
#else
#error "Unknown platform"
#endif
}

#if CXXTRACE_HAVE_PROC_PIDINFO
auto
thread_name_set::fetch_and_remember_name_of_current_thread_libproc(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());
  this->fetch_and_remember_thread_name_for_id_libproc(current_thread_id);
}
#endif

auto
thread_name_set::fetch_and_remember_thread_name_for_id(thread_id id) noexcept(
  false) -> void
{
#if CXXTRACE_HAVE_PROC_PIDINFO
  this->fetch_and_remember_thread_name_for_id_libproc(id);
#elif CXXTRACE_HAVE_LINUX_PROCFS
  this->fetch_and_remember_thread_name_for_id_procfs(id);
#else
#error "Unknown platform"
#endif
}

#if CXXTRACE_HAVE_PROC_PIDINFO
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
#endif

#if CXXTRACE_HAVE_LINUX_PROCFS
auto
thread_name_set::fetch_and_remember_thread_name_for_id_procfs(
  thread_id id) noexcept(false) -> void
{
  auto path = stringify("/proc/self/task/", id, "/comm");
  auto fd = file_descriptor{ ::open(path.data(), O_CLOEXEC | O_RDONLY) };
  if (!fd.valid()) {
    auto error = errno;
    if (error == ENOENT) {
      // The thread is dead, or was never alive. Ignore it.
      // TODO(strager): Should we communicate this fact to the caller?
    } else {
      // FIXME(strager): How should we handle this error? Should we throw an
      // exception?
      std::fprintf(
        stderr,
        "error: could not get name of current thread by opening %s: %s\n",
        path.data(),
        std::strerror(error));
    }
    return;
  }

  auto& thread_name = this->allocate_name(id, max_thread_name_length);
  auto thread_name_length =
    ::read(fd.get(), thread_name.data(), thread_name.size());
  if (thread_name_length == -1) {
    auto error = errno;
    // FIXME(strager): How should we handle this error? Should we throw an
    // exception?
    std::fprintf(stderr,
                 "error: could not get name of current thread from %s: %s\n",
                 path.data(),
                 std::strerror(error));
    return;
  }
  assert(std::size_t(thread_name_length) <= thread_name.size());
  auto end_of_line_index = thread_name.find('\n');
  if (end_of_line_index != thread_name.npos) {
    thread_name_length = end_of_line_index;
  }
  thread_name.resize(thread_name_length);
}
#endif

#if CXXTRACE_HAVE_MACH_THREAD
auto
thread_name_set::fetch_and_remember_name_of_current_thread_mach(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());

  auto thread_port = mach_port{ ::mach_thread_self() };
  auto info = get_thread_extended_info_or_terminate(thread_port.port);
  auto thread_name_length = ::strnlen(info.pth_name, sizeof(info.pth_name));

  auto [it, inserted] = this->names.try_emplace(
    current_thread_id, info.pth_name, thread_name_length);
  if (!inserted) {
    it->second.assign(info.pth_name, thread_name_length);
  }
}
#endif

#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP
auto
thread_name_set::fetch_and_remember_name_of_current_thread_pthread(
  thread_id current_thread_id) noexcept(false) -> void
{
  assert(current_thread_id == get_current_thread_id());

  auto& thread_name =
    this->allocate_name(current_thread_id, max_thread_name_length);
  [[maybe_unused]] auto rc = ::pthread_getname_np(
    ::pthread_self(), thread_name.data(), thread_name.size() + 1);
  assert(rc == 0);
  assert(thread_name.data()[thread_name.size()] == '\0');
  thread_name.resize(std::strlen(thread_name.c_str()));
}
#endif

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

auto
thread_name_set::allocate_name(thread_id id,
                               std::size_t max_name_length) noexcept(false)
  -> std::string&
{
  auto [it, inserted] = this->names.try_emplace(id, max_name_length, '\0');
  std::string& name = it->second;
  if (!inserted) {
    name.resize(max_name_length);
  }
  assert(name.size() >= max_name_length);
  return name;
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
