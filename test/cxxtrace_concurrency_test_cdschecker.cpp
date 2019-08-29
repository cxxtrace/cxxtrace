#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cxxtrace_concurrency_test.h"
#include <cxxtrace/detail/mutex.h>
#include <cxxtrace/detail/workarounds.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <threads.h>
#include <utility>
#include <vector>
// IWYU pragma: no_include <new>

namespace {
template<class Func>
auto spawn_thread(Func &&) -> ::thrd_t;

auto
join_thread(::thrd_t thread) -> void;
}

namespace cxxtrace_test {
cdschecker_backoff::cdschecker_backoff() = default;

cdschecker_backoff::~cdschecker_backoff() = default;

auto
cdschecker_backoff::reset() -> void
{}

auto cdschecker_backoff::yield(cxxtrace::detail::debug_source_location) -> void
{
  ::thrd_yield();
}

namespace detail {
auto
run_concurrency_test_from_cdschecker(detail::concurrency_test* test) -> void
{
  test->set_up();

  auto threads = std::vector<::thrd_t>{};
  threads.reserve(test->thread_count());
  for (auto i = 0; i < test->thread_count(); ++i) {
    threads.emplace_back(spawn_thread([i, test] { test->run_thread(i); }));
  }
  for (auto& thread : threads) {
    join_thread(thread);
  }

  test->tear_down();
}
}

auto
concurrency_log(void (*make_message)(std::ostream&, void* opaque),
                void* opaque,
                cxxtrace::detail::debug_source_location) -> void
{
  auto& out = std::cout;
  make_message(out, opaque);
  out << '\n';
}
}

namespace {
template<class Func>
auto
spawn_thread(Func&& routine) -> ::thrd_t
{
  auto unique_routine = std::make_unique<Func>(std::forward<Func&&>(routine));

  auto call_routine = [](void* opaque) noexcept->auto
  {
    auto routine = std::unique_ptr<Func>{ static_cast<Func*>(opaque) };
    (*routine)();
#if CXXTRACE_WORK_AROUND_CDCHECKER_THRD_START_T
    return /*void*/;
#else
    return 0;
#endif
  };

  auto thread = ::thrd_t{};
  [[maybe_unused]] auto rc =
    ::thrd_create(&thread, call_routine, unique_routine.get());
#if CXXTRACE_WORK_AROUND_MISSING_THRD_ENUM
  if (rc != 0) {
    throw std::runtime_error{ "thrd_create failed" };
  }
#else
  if (rc != ::thrd_success) {
    throw std::runtime_error{ "thrd_create failed" };
  }
#endif
  unique_routine.release();

  return thread;
}

auto
join_thread(::thrd_t thread) -> void
{
#if CXXTRACE_WORK_AROUND_CDCHECKER_THRD_JOIN
  auto rc = ::thrd_join(thread);
  if (rc != 0) {
    throw std::runtime_error{ "thrd_join failed" };
  }
#else
  auto rc = ::thrd_join(thread, nullptr);
  if (rc != ::thrd_success) {
    throw std::runtime_error{ "thrd_join failed" };
  }
#endif
}
}

namespace cxxtrace {
namespace detail {
// NOTE(strager): In this translation unit, std::mutex refers to CDSChecker's
// mutex, not the standard library's mutex.
using cdschecker_mutex_impl = std::mutex;

static_assert(alignof(std::mutex_state) == alignof(cdschecker_mutex_impl));
static_assert(sizeof(std::mutex_state) == sizeof(cdschecker_mutex_impl));

cdschecker_mutex::cdschecker_mutex()
{
  new (&this->get()) cdschecker_mutex_impl{};
}

cdschecker_mutex::~cdschecker_mutex()
{
  this->get().~mutex();
}

auto cdschecker_mutex::lock(debug_source_location) noexcept -> void
{
  this->get().lock();
}

auto cdschecker_mutex::unlock(debug_source_location) noexcept -> void
{
  this->get().unlock();
}

inline auto
cdschecker_mutex::get() noexcept -> cdschecker_mutex_impl&
{
  static_assert(alignof(decltype(this->storage_)) ==
                alignof(cdschecker_mutex_impl));
  static_assert(sizeof(decltype(this->storage_)) ==
                sizeof(cdschecker_mutex_impl));

  return *reinterpret_cast<cdschecker_mutex_impl*>(&this->storage_);
}
}
}
