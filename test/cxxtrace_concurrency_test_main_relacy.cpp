#include "cxxtrace_concurrency_test_base.h"
#include <cstdlib>

int
main()
{
  cxxtrace::register_concurrency_tests();
  for (auto* test : cxxtrace::detail::get_registered_concurrency_tests()) {
    cxxtrace::detail::run_concurrency_test_with_relacy(test);
  }
  return EXIT_SUCCESS;
}
