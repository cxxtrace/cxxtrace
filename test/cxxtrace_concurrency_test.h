#ifndef CXXTRACE_CONCURRENCY_TEST_H
#define CXXTRACE_CONCURRENCY_TEST_H

#include "cxxtrace_concurrency_test_base.h" // IWYU pragma: export
#include "pretty_type_name.h"
#include <cassert>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <exception>
#include <experimental/memory_resource>
#include <iosfwd>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#if CXXTRACE_ENABLE_CDSCHECKER
#include <cxxtrace/detail/cdschecker.h>
#include <cxxtrace/string.h>
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/string.h>
#endif

#if CXXTRACE_ENABLE_RELACY
#include <cxxtrace/detail/warning.h>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdynamic-exception-spec")
CXXTRACE_WARNING_IGNORE_CLANG("-Wextra-semi")
CXXTRACE_WARNING_IGNORE_CLANG("-Winline-new-delete")
CXXTRACE_WARNING_IGNORE_CLANG("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_GCC("-Wsized-deallocation")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")

// clang-format off
#include <relacy/thread_local_ctx.hpp>
// clang-format on
#include <relacy/backoff.hpp>
#include <relacy/context.hpp>
#include <relacy/context_base_impl.hpp>
#include <relacy/stdlib/condition_variable.hpp>
#include <relacy/stdlib/event.hpp>
#include <relacy/stdlib/mutex.hpp>

CXXTRACE_WARNING_POP
#endif

#if CXXTRACE_ENABLE_CDSCHECKER
#define CXXTRACE_ASSERT(...)                                                   \
  do {                                                                         \
    if (!(__VA_ARGS__)) {                                                      \
      ::cxxtrace_test::detail::cdschecker_assert_failure(__FILE__, __LINE__);  \
    }                                                                          \
  } while (false)
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#define CXXTRACE_ASSERT(...)                                                   \
  do {                                                                         \
    if (!(__VA_ARGS__)) {                                                      \
      ::cxxtrace_test::detail::concurrency_stress_assert(                      \
        #__VA_ARGS__, __FILE__, __LINE__);                                     \
    }                                                                          \
  } while (false)
#endif

#if CXXTRACE_ENABLE_RELACY
#define CXXTRACE_ASSERT(...) RL_ASSERT((__VA_ARGS__))
#endif

namespace cxxtrace_test {
namespace detail {
extern std::experimental::pmr::memory_resource* concurrency_tests_memory;
}

template<class Test, class... Args>
auto
register_concurrency_test(int thread_count,
                          concurrency_test_depth depth,
                          Args... args)
{
  class test : public detail::concurrency_test
  {
  public:
    explicit test(int thread_count,
                  concurrency_test_depth depth,
                  const Args&... args)
      : args{ args... }
      , thread_count_{ thread_count }
      , depth_{ depth }
    {}

    ~test() = default;

    auto set_up() -> void override
    {
      // assert(!this->test_object.has_value()); // @@@
      new (&this->test_object) std::optional<Test>{}; // @@@
      std::apply(
        [this](const Args&... args) { this->test_object.emplace(args...); },
        this->args);
    }

    auto run_thread(int thread_index) -> void override
    {
      assert(this->test_object.has_value());
      this->test_object->run_thread(thread_index);
    }

    auto tear_down() -> void override
    {
      assert(this->test_object.has_value());
      this->test_object->tear_down();
      this->test_object.reset();
    }

    auto test_depth() const noexcept -> concurrency_test_depth override
    {
      return this->depth_;
    }

    auto thread_count() const noexcept -> int override
    {
      return this->thread_count_;
    }

    auto name() const -> std::string override
    {
      return pretty_type_name(typeid(Test));
    }

  private:
    std::tuple<Args...> args;
    int thread_count_;
    concurrency_test_depth depth_;
    std::optional<Test> test_object{};
  };

  auto* t = static_cast<test*>(
    detail::concurrency_tests_memory->allocate(sizeof(test), alignof(test)));
  new (t) test(thread_count, depth, args...);

  detail::register_concurrency_test(t);
}

#if CXXTRACE_ENABLE_CDSCHECKER
class cdschecker_backoff
{
public:
  explicit cdschecker_backoff();
  ~cdschecker_backoff();

  auto reset() -> void;
  auto yield(cxxtrace::detail::debug_source_location) -> void;
};
#endif

#if CXXTRACE_ENABLE_RELACY
class relacy_backoff
{
public:
  explicit relacy_backoff();
  ~relacy_backoff();

  auto reset() -> void;
  auto yield(cxxtrace::detail::debug_source_location) -> void;

private:
  rl::linear_backoff backoff;
};
#endif

#if CXXTRACE_ENABLE_CDSCHECKER
using backoff = cdschecker_backoff;
#elif CXXTRACE_ENABLE_CONCURRENCY_STRESS
using backoff = cxxtrace::detail::backoff;
#elif CXXTRACE_ENABLE_RELACY
using backoff = relacy_backoff;
#else
#error "Unknown configuration"
#endif

auto
concurrency_log(void (*)(std::ostream&, void* opaque),
                void* opaque,
                cxxtrace::detail::debug_source_location) -> void;

template<class Func>
auto
concurrency_log(Func&& func, cxxtrace::detail::debug_source_location caller)
  -> void
{
  concurrency_log(
    [](std::ostream& out, void* opaque) {
      auto func = static_cast<Func*>(opaque);
      std::forward<Func> (*func)(out);
    },
    &func,
    caller);
}

auto
concurrency_rng_next_integer_0(int max_plus_one) noexcept -> int;

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
using concurrency_stress_generation = std::int64_t;

auto
current_concurrency_stress_generation() noexcept
  -> concurrency_stress_generation;
#endif

namespace detail {
#if CXXTRACE_ENABLE_CDSCHECKER
auto
cdschecker_assert_failure(cxxtrace::czstring file_name, int line_number)
  -> void;
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
auto
concurrency_stress_assert(cxxtrace::czstring message,
                          cxxtrace::czstring file_name,
                          int line_number) -> void;
#endif
}
}

#endif
