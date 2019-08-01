#include "cxxtrace_concurrency_test_base.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace cxxtrace_test {
namespace detail {
namespace {
std::vector<std::unique_ptr<concurrency_test>> registered_concurrency_tests{};
}

concurrency_test::~concurrency_test() = default;

auto
register_concurrency_test(std::unique_ptr<concurrency_test> test) -> void
{
  registered_concurrency_tests.emplace_back(std::move(test));
}

auto
get_registered_concurrency_tests() -> std::vector<concurrency_test*>
{
  auto tests = std::vector<concurrency_test*>{};
  std::transform(
    registered_concurrency_tests.begin(),
    registered_concurrency_tests.end(),
    std::back_inserter(tests),
    [](const std::unique_ptr<concurrency_test>& test) { return test.get(); });
  return tests;
}
}
}
