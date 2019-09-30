#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include <cstring>
#include <cxxtrace/detail/cdschecker.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/string.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
template<class Func>
auto spawn_thread(Func &&) -> cxxtrace::detail::cdschecker::thrd_t;

auto
join_thread(cxxtrace::detail::cdschecker::thrd_t thread) -> void;
}

namespace cxxtrace_test {
cdschecker_backoff::cdschecker_backoff() = default;

cdschecker_backoff::~cdschecker_backoff() = default;

auto
cdschecker_backoff::reset() -> void
{}

auto cdschecker_backoff::yield(cxxtrace::detail::debug_source_location) -> void
{
  cxxtrace::detail::cdschecker::thrd_yield();
}

namespace detail {
auto
run_concurrency_test_from_cdschecker(detail::concurrency_test* test) -> void
{
  test->set_up();

  auto threads = std::vector<cxxtrace::detail::cdschecker::thrd_t>{};
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

namespace detail {
auto
cdschecker_assert_failure(cxxtrace::czstring file_name, int line_number) -> void
{
  // HACK(strager): model_assert's assertion message buffer has a fixed size,
  // and CDSChecker uses an unchecked sprintf to write the message. To avoid
  // undefined behavior, truncate the file name to hopefully fit the assertion
  // message within the buffer's bounds.
  constexpr auto max_file_name_length = 40;
  auto file_name_length = std::strlen(file_name);
  auto truncated_file_name =
    file_name_length > max_file_name_length
      ? file_name + file_name_length - max_file_name_length
      : file_name;

  cxxtrace::detail::cdschecker::model_assert(
    false, truncated_file_name, line_number);
}
}
}

namespace {
template<class Func>
auto
spawn_thread(Func&& routine) -> cxxtrace::detail::cdschecker::thrd_t
{
  auto unique_routine = std::make_unique<Func>(std::forward<Func&&>(routine));

  auto call_routine = [](void* opaque) noexcept->void
  {
    auto routine = std::unique_ptr<Func>{ static_cast<Func*>(opaque) };
    (*routine)();
  };

  auto thread = cxxtrace::detail::cdschecker::thrd_t{};
  auto rc = cxxtrace::detail::cdschecker::thrd_create(
    &thread, call_routine, unique_routine.get());
  if (rc != 0) {
    throw std::runtime_error{ "thrd_create failed" };
  }
  unique_routine.release();

  return thread;
}

auto
join_thread(cxxtrace::detail::cdschecker::thrd_t thread) -> void
{
  auto rc = cxxtrace::detail::cdschecker::thrd_join(thread);
  if (rc != 0) {
    throw std::runtime_error{ "thrd_join failed" };
  }
}
}
