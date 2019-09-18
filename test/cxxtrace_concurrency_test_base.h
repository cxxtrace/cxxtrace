#ifndef CXXTRACE_CONCURRENCY_TEST_BASE_H
#define CXXTRACE_CONCURRENCY_TEST_BASE_H

#include <string>
#include <vector>

namespace cxxtrace_test {
// Define this function in each test.
auto
register_concurrency_tests() -> void;

enum class concurrency_test_depth
{
  shallow,
  full,
};

namespace detail {
class concurrency_test
{
public:
  virtual ~concurrency_test() = default;

  virtual auto set_up() -> void = 0;
  virtual auto run_thread(int thread_index) -> void = 0;
  virtual auto tear_down() -> void = 0;

  virtual auto test_depth() const noexcept -> concurrency_test_depth = 0;
  virtual auto thread_count() const noexcept -> int = 0;

  virtual auto name() const -> std::string;
};

auto
register_concurrency_test(concurrency_test*) -> void;

auto
get_registered_concurrency_tests() -> std::vector<concurrency_test*>;

auto
run_concurrency_test_from_cdschecker(concurrency_test*) -> void;

auto
run_concurrency_test_with_relacy(concurrency_test*) -> void;
}
}

#endif
