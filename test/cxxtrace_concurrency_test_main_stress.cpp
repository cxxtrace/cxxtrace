#if !CXXTRACE_ENABLE_CONCURRENCY_STRESS
#error "CXXTRACE_ENABLE_CONCURRENCY_STRESS must be defined and non-zero."
#endif

#include "concurrency_test_runner.h"
#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/string.h>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace {
auto
stress_run_concurrency_test(cxxtrace_test::detail::concurrency_test*) -> void;

cxxtrace_test::concurrency_test_runner* current_test_runner{ nullptr };
std::atomic<bool> thread_sanitizer_failed{ false };
}

auto
main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) -> int
{
  cxxtrace_test::register_concurrency_tests();
  for (auto* test : cxxtrace_test::detail::get_registered_concurrency_tests()) {
    stress_run_concurrency_test(test);
  }
}

namespace {
auto
stress_run_concurrency_test(cxxtrace_test::detail::concurrency_test* test)
  -> void
{
  using clock = std::chrono::steady_clock;

  auto runner = cxxtrace_test::concurrency_test_runner{ test };
  current_test_runner = &runner;
  runner.start_workers();

  std::cerr << "Running " << test->name() << "...\n";

  auto failed = false;
  auto deadline = clock::now() + 500ms;
  while (!failed && clock::now() < deadline) {
    auto rounds_between_deadline_checks = 3;
    for (auto i = 0; !failed && i < rounds_between_deadline_checks; ++i) {
      runner.run_once();
      auto failures = runner.failures();
      if (!failures.empty()) {
        for (const auto& failure : failures) {
          std::cerr << failure << '\n';
        }
        failed = true;
      }
      if (thread_sanitizer_failed.load()) {
        std::cerr << "error: ThreadSanitizer detected a problem\n";
        failed = true;
      }
    }
  }
  runner.stop_workers();
  current_test_runner = nullptr;

  std::cerr << (failed ? "Failed" : "Passed") << " after " << runner.run_count()
            << " test executions\n";
  if (failed) {
    std::exit(1);
  }
}
}

namespace cxxtrace_test {
auto
concurrency_log([[maybe_unused]] void (*make_message)(std::ostream&,
                                                      void* opaque),
                [[maybe_unused]] void* opaque,
                cxxtrace::detail::debug_source_location) -> void
{
  // TODO(strager): Log into a temporary buffer, and dump the buffer on failure.
  // We should probably do this probabilistically to avoid interfering with the
  // execution of the test.
}

namespace detail {
auto
concurrency_stress_assert(cxxtrace::czstring message,
                          cxxtrace::czstring file_name,
                          int line_number) -> void
{
  auto full_message_stream = std::ostringstream{};
  full_message_stream << "error: " << file_name << ":" << line_number
                      << ": assertion failed: " << message;
  auto full_message = std::move(full_message_stream).str();
  if (current_test_runner) {
    current_test_runner->add_failure(full_message.c_str());
  } else {
    std::cerr << full_message
              << "\nfatal: assertion failed when not running a test\n";
    std::terminate();
  }
}
}
}

namespace __tsan {
class ReportDesc;
}

extern "C" auto
__tsan_on_report(const __tsan::ReportDesc*) -> void;

extern "C" auto
__tsan_on_report(const __tsan::ReportDesc*) -> void
{
  thread_sanitizer_failed.store(true);
}
