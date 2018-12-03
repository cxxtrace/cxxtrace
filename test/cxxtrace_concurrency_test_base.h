#ifndef CXXTRACE_CONCURRENCY_TEST_BASE_H
#define CXXTRACE_CONCURRENCY_TEST_BASE_H

#include <memory>
#include <vector>

namespace cxxtrace {
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
  virtual ~concurrency_test();

  virtual auto set_up() -> void = 0;
  virtual auto run_thread(int thread_index) -> void = 0;
  virtual auto tear_down() -> void = 0;

  virtual auto test_depth() const noexcept -> concurrency_test_depth = 0;
  virtual auto thread_count() const noexcept -> int = 0;
};

auto register_concurrency_test(std::unique_ptr<concurrency_test>) -> void;

auto
get_registered_concurrency_tests() -> std::vector<concurrency_test*>;

auto
run_concurrency_test_with_relacy(concurrency_test*) -> void;
}
}

#endif
