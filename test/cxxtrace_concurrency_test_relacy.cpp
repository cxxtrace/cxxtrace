#if !CXXTRACE_ENABLE_RELACY
#error "CXXTRACE_ENABLE_RELACY must be defined and non-zero."
#endif

#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "memory_resource.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>
#include <exception>
#include <experimental/memory_resource>
#include <experimental/string>
#include <new>
#include <ostream>
#include <sstream>
#include <utility>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <type_traits>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wextra-semi")
CXXTRACE_WARNING_IGNORE_CLANG("-Winline-new-delete")
CXXTRACE_WARNING_IGNORE_CLANG("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_GCC("-Wsized-deallocation")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")

// Needed by <relacy/context_base.hpp>.
#include <relacy/thread_local_ctx.hpp> // IWYU pragma: keep

#include <relacy/backoff.hpp>
#include <relacy/context.hpp>
#include <relacy/context_base.hpp>
#include <relacy/context_base_impl.hpp>
#include <relacy/test_params.hpp>
#include <relacy/test_suite.hpp>

CXXTRACE_WARNING_POP

namespace cxxtrace_test {
namespace detail {
namespace {
template<int ThreadCount>
auto
run_concurrency_test_with_relacy_with_threads(concurrency_test* test) -> void;
}

auto
run_concurrency_test_with_relacy(concurrency_test* test) -> void
{
#define CASE(thread_count)                                                     \
  case (thread_count):                                                         \
    run_concurrency_test_with_relacy_with_threads<(thread_count)>(test);       \
    break;
  switch (test->thread_count()) {
    CASE(1)
    CASE(2)
    CASE(3)
    default:
      assert(false && "Unsupport thread count");
      break;
  }
#undef CASE
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
  *options.output_stream << "Testing " << test->name() << "...\n";
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

relacy_backoff::relacy_backoff() = default;

relacy_backoff::~relacy_backoff() = default;

auto
relacy_backoff::reset() -> void
{
  this->backoff = rl::linear_backoff{};
}

auto
relacy_backoff::yield(cxxtrace::detail::debug_source_location caller) -> void
{
  this->backoff.yield(caller);
}

auto
concurrency_log(void (*make_message)(std::ostream&, void* opaque),
                void* opaque,
                cxxtrace::detail::debug_source_location caller) -> void
{
  auto& relacy_context = rl::ctx();
  if (!relacy_context.collecting_history()) {
    return;
  }

  using pmr_ostringstream = std::basic_ostringstream<
    char,
    std::char_traits<char>,
    std::experimental::pmr::polymorphic_allocator<char>>;

  auto buffer = std::array<std::byte, 1024>{};
  auto memory = monotonic_buffer_resource{ buffer.data(), buffer.size() };
  auto message_stream =
    pmr_ostringstream{ std::experimental::pmr::string{ &memory } };
  make_message(message_stream, opaque);
  auto message_string = std::move(message_stream).str();
  relacy_context.exec_log(
    caller,
    rl::user_msg_event{ { message_string.begin(), message_string.end() } });
}

auto
concurrency_rng_next_integer_0(int max_plus_one) noexcept -> int
{
  return rl::rand(max_plus_one);
}
}
