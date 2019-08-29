#include "cxxtrace_concurrency_test_base.h"
#include <cstdlib>
// IWYU pragma: no_include <vector>

int
main()
{
  cxxtrace_test::register_concurrency_tests();
  for (auto* test : cxxtrace_test::detail::get_registered_concurrency_tests()) {
    cxxtrace_test::detail::run_concurrency_test_with_relacy(test);
  }
  return EXIT_SUCCESS;
}
