#include "cxxtrace_concurrency_test_base.h"
#include <experimental/memory_resource>
#include <vector>

#if defined(__linux__)
#include "memory_resource.h"
#include <array>
#include <cstddef>
#endif

namespace cxxtrace_test {
namespace detail {
namespace {
auto registered_concurrency_tests = std::vector<concurrency_test*>{};
}

#if defined(__APPLE__)
// HACK(strager): CDSChecker handles pre-user_main allocations poorly. Put
// concurrency_test objects in libCDSChecker_model.dylib's bootstrapmemory
// global variable in the hopes that CDSChecker snapshots this memory correctly.
std::experimental::pmr::memory_resource* concurrency_tests_memory =
  std::experimental::pmr::new_delete_resource();
#endif

#if defined(__linux__)
namespace {
// HACK(strager): CDSChecker handles pre-user_main allocations poorly. Put
// concurrency_test objects in our executable's .data section in the hopes that
// CDSChecker snapshots this memory correctly.
auto registered_concurrency_tests_storage =
  std::array<std::byte, 256 * 1024>{ std::byte{ 42 } };
auto registered_concurrency_tests_memory =
  monotonic_buffer_resource{ registered_concurrency_tests_storage.data(),
                             registered_concurrency_tests_storage.size() };
}

std::experimental::pmr::memory_resource* concurrency_tests_memory =
  &registered_concurrency_tests_memory;
#endif

auto
register_concurrency_test(concurrency_test* test) -> void
{
  registered_concurrency_tests.emplace_back(test);
}

auto
get_registered_concurrency_tests() -> std::vector<concurrency_test*>
{
  return registered_concurrency_tests;
}
}
}
