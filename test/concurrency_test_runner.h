#ifndef CXXTRACE_TEST_CONCURRENCY_TEST_RUNNER_H
#define CXXTRACE_TEST_CONCURRENCY_TEST_RUNNER_H

#include <cxxtrace/string.h>
#include <memory>
#include <string>
#include <vector>

namespace cxxtrace_test {
namespace detail {
class concurrency_test;
}

class concurrency_test_runner
{
public:
  explicit concurrency_test_runner(detail::concurrency_test* test);

  concurrency_test_runner(const concurrency_test_runner&) = delete;
  concurrency_test_runner& operator=(const concurrency_test_runner&) = delete;

  ~concurrency_test_runner();

  auto start_workers() -> void;
  auto run_once() -> void;
  auto stop_workers() -> void;

  auto run_count() -> long;

  auto add_failure(cxxtrace::czstring message) -> void;
  auto failures() const -> std::vector<std::string>;

private:
  class impl;

  std::unique_ptr<impl> impl_;
};
}

#endif
