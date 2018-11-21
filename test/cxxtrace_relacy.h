#ifndef CXXTRACE_RELACY_H
#define CXXTRACE_RELACY_H

#if !CXXTRACE_ENABLE_RELACY
#error "CXXTRACE_ENABLE_RELACY must be defined and non-zero."
#endif

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <tuple>
#include <utility>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wdynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winline-new-delete"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-parameter"

// clang-format off
#include <relacy/thread_local_ctx.hpp>
// clang-format on
#include <relacy/backoff.hpp>
#include <relacy/context.hpp>
#include <relacy/context_base_impl.hpp>
#include <relacy/stdlib/condition_variable.hpp>
#include <relacy/stdlib/event.hpp>
#include <relacy/stdlib/mutex.hpp>

#pragma clang diagnostic pop

namespace cxxtrace {
namespace detail {
template<int ThreadCount>
class relacy_test_base
  : public rl::test_suite<relacy_test_base<ThreadCount>, ThreadCount>
{};

// HACK(strager): rl::simulate does not let us customize how the test object is
// constructed. Work around this limitation by storing constructor parameters in
// a global variable.
template<int ThreadCount, class Test, class... Args>
class relacy_test_wrapper : public relacy_test_base<ThreadCount>
{
public:
  inline static std::optional<std::tuple<Args...>> global_args = {};

  explicit relacy_test_wrapper()
    : relacy_test_wrapper{ std::index_sequence_for<Args...>{} }
  {}

  auto thread(unsigned thread_index) -> void
  {
    this->test.thread(thread_index);
  }

private:
  template<std::size_t... Indexes>
  explicit relacy_test_wrapper(std::index_sequence<Indexes...>)
    : test{ std::get<Indexes>(*global_args)... }
  {}

  Test test;
};
}

template<int ThreadCount, class Test, class... Args>
auto
run_relacy_test(rl::scheduler_type_e search_type, const Args&... args) noexcept
  -> void
{
  using test_wrapper = detail::relacy_test_wrapper<ThreadCount, Test, Args...>;

  auto options = rl::test_params{};
  options.context_bound = 3;
  options.search_type = search_type;

  test_wrapper::global_args.emplace(args...);
  auto succeeded = rl::simulate<test_wrapper>(options);
  test_wrapper::global_args.reset();

  if (!succeeded) {
    std::exit(EXIT_FAILURE);
  }
}
}

#endif
