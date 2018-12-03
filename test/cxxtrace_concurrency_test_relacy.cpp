#if !CXXTRACE_ENABLE_RELACY
#error "CXXTRACE_ENABLE_RELACY must be defined and non-zero."
#endif

#include "cxxtrace_concurrency_test.h"
#include <cassert>
#include <cstdlib>
#include <exception>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winline-new-delete"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <relacy/context.hpp>
#include <relacy/context_base_impl.hpp>

#pragma clang diagnostic pop

namespace cxxtrace {
namespace detail {
namespace {
template<int ThreadCount>
auto
run_concurrency_test_with_relacy_with_threads(concurrency_test* test) -> void;
}

auto
run_concurrency_test_with_relacy(concurrency_test* test) -> void
{
  if (test->thread_count() == 2) {
    run_concurrency_test_with_relacy_with_threads<2>(test);
  } else {
    assert(false && "Unsupport thread count");
  }
}

namespace {
template<int ThreadCount>
class concurrency_test_relacy_wrapper;

auto
relacy_search_type(concurrency_test_depth depth) noexcept
  -> rl::scheduler_type_e
{
  switch (depth) {
    case concurrency_test_depth::shallow:
      return rl::fair_context_bound_scheduler_type;
    case concurrency_test_depth::full:
      return rl::fair_full_search_scheduler_type;
  }
  std::terminate();
}

template<int ThreadCount>
auto
run_concurrency_test_with_relacy_with_threads(concurrency_test* test) -> void
{
  using test_wrapper = concurrency_test_relacy_wrapper<ThreadCount>;

  auto options = rl::test_params{};
  options.context_bound = 3;
  options.search_type = relacy_search_type(test->test_depth());

  test_wrapper::test = test;
  auto succeeded = rl::simulate<test_wrapper>(options);
  test_wrapper::test = nullptr;

  if (!succeeded) {
    std::exit(EXIT_FAILURE);
  }
}

template<int ThreadCount>
class concurrency_test_relacy_wrapper
  : public rl::test_suite<concurrency_test_relacy_wrapper<ThreadCount>,
                          ThreadCount>
{
public:
  inline static concurrency_test* test{ nullptr };

  auto before() -> void
  {
    assert(this->test);
    this->test->set_up();
  }

  auto after() -> void
  {
    assert(this->test);
    this->test->tear_down();
  }

  auto thread(unsigned thread_index) -> void
  {
    assert(this->test);
    this->test->run_thread(thread_index);
  }

  auto invariant() -> void
  {
    // Do nothing.
  }
};
}
}

#if CXXTRACE_ENABLE_RELACY
relacy_backoff::relacy_backoff() = default;

relacy_backoff::~relacy_backoff() = default;

auto
relacy_backoff::yield(detail::debug_source_location caller) -> void
{
  this->backoff.yield(caller);
}
#endif
}
