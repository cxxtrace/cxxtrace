#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/detail/workarounds.h>
#include <cxxtrace/thread.h>
#include <string>
#include <type_traits>

#if defined(__APPLE__) && defined(__MACH__)
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/apple_commpage.h>
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

class mach_port
{
public:
  explicit mach_port(::mach_port_t port) noexcept
    : port{ port }
  {}

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
  auto thread_port = mach_port{ ::mach_thread_self() };
  auto info = get_thread_identifier_info_or_terminate(thread_port.port);
  return thread_id{ info.thread_id };
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

  auto thread_port = mach_port{ ::mach_thread_self() };
  auto info = get_thread_extended_info_or_terminate(thread_port.port);
  auto thread_name_length = ::strnlen(info.pth_name, sizeof(info.pth_name));

  auto [it, inserted] = this->names.try_emplace(
    current_thread_id, info.pth_name, thread_name_length);
  if (!inserted) {
    it->second.assign(info.pth_name, thread_name_length);
  }
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
processor_id_lookup_x86_cpuid_uncached ::get_current_processor_id() const
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
processor_id_lookup_x86_cpuid_commpage_preempt_cached::
  get_current_processor_id() const noexcept -> processor_id
{
  auto* c_ptr = &this->thread_local_cache;
#if CXXTRACE_WORK_AROUND_THREAD_LOCAL_OPTIMIZER
  asm volatile("" : "+r"(c_ptr));
#endif
  auto& c = *c_ptr;

#if CXXTRACE_CHECK_COMMPAGE_SIGNATURE_AND_VERSION
  if (!this->commpage_supported) {
    return this->uncached_lookup.get_current_processor_id();
  }
#endif

#if CXXTRACE_WORK_AROUND_ATOMIC_VALUE_TYPE
  using sched_gen_type = std::uint32_t;
#else
  using sched_gen_type =
    std::decay_t<decltype(*apple_commpage::sched_gen)>::value_type;
#endif
  using scheduler_generation_type =
    decltype(c.scheduler_generation_and_initialized);
  static_assert(
    (sched_gen_type{ ~0U } | cache::initialized) != sched_gen_type{ ~0U },
    "cache::initialized must be disjoint from *apple_commpage::sched_gen");

  auto scheduler_generation = scheduler_generation_type{
    apple_commpage::sched_gen->load(std::memory_order_seq_cst)
  };
  // TODO(strager): Determine if a separate boolean check is faster than
  // juggling the cache::initialized bit.
  scheduler_generation |= cache::initialized;
  if (scheduler_generation == c.scheduler_generation_and_initialized) {
    // This thread was not rescheduled onto a different processor. Our cached
    // processor ID is valid.
  } else {
    // Either this is the first time we're being called on this thread, or this
    // thread was rescheduled onto a different processor, or another thread on
    // the system was scheduled onto a different processor. Our cached processor
    // ID is possibly invalid.
    c.id = this->uncached_lookup.get_current_processor_id();
    c.scheduler_generation_and_initialized = scheduler_generation;
    // NOTE(strager): If *apple_commpage::sched_gen changes again (i.e.
    // disagrees with our scheduler_generation automatic variable), ideally we
    // would query the processor ID again. However, this function is a hint
    // anyway; there's no point in getting the "right" answer when it'll be
    // possibly invalid after returning anyway.
  }
  return c.id;
}

thread_local processor_id_lookup_x86_cpuid_commpage_preempt_cached::cache
  processor_id_lookup_x86_cpuid_commpage_preempt_cached::thread_local_cache;
#endif
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
