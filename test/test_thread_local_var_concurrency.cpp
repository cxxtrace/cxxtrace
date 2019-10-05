#include "cxxtrace_concurrency_test.h"
#include "cxxtrace_concurrency_test_base.h"
#include "thread_local_var.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cxxtrace/detail/workarounds.h>
#include <string>

#if CXXTRACE_ENABLE_RELACY
#include <relacy/context.hpp>
#include <relacy/context_base.hpp>
#endif

namespace cxxtrace_test {
class test_thread_local_variable_is_zero_on_thread_entry
{
public:
  auto run_thread([[maybe_unused]] int thread_index) -> void
  {
    CXXTRACE_ASSERT(*this->var == 0);

#if !CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
    // Coerce model checkers into executing another iteration.
    concurrency_rng_next_integer_0(2);
#endif

    *this->var = 1;
#if !CXXTRACE_WORK_AROUND_CDSCHECKER_DETERMINISM
    // If this->var is not reset between iterations, the assert above will fail.
#endif
  }

  auto tear_down() -> void {}

  // NOTE(strager): var is intentionally not a class member variable. The test
  // framework resets member variables automatically, but does not necessarily
  // reset static variables automatically.
  static inline thread_local_var<int> var{};
};

class test_thread_local_variable_is_a_normal_variable_on_one_thread
{
public:
  auto run_thread([[maybe_unused]] int thread_index) -> void
  {
    *this->var = 1;
    CXXTRACE_ASSERT(*this->var == 1);

    *this->var = 3;
    CXXTRACE_ASSERT(*this->var == 3);
  }

  auto tear_down() -> void {}

  thread_local_var<int> var{};
};

class test_thread_local_variables_are_thread_independent
{
public:
  auto run_thread(int thread_index) -> void
  {
    assert(thread_index < int(var_pointers.size()));
    this->var_pointers[thread_index] =
      reinterpret_cast<std::uintptr_t>(&*this->var);
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->var_pointers[0] != this->var_pointers[1]);
    CXXTRACE_ASSERT(this->var_pointers[1] != this->var_pointers[2]);
    CXXTRACE_ASSERT(this->var_pointers[2] != this->var_pointers[0]);
  }

  std::array<std::uintptr_t, 3> var_pointers;
  thread_local_var<int> var{};
};

class test_static_thread_local_variables_are_thread_independent
{
public:
  auto run_thread(int thread_index) -> void
  {
    assert(thread_index < int(var_pointers.size()));
    this->var_pointers[thread_index] =
      reinterpret_cast<std::uintptr_t>(&*this->var);
  }

  auto tear_down() -> void
  {
    CXXTRACE_ASSERT(this->var_pointers[0] != this->var_pointers[1]);
    CXXTRACE_ASSERT(this->var_pointers[1] != this->var_pointers[2]);
    CXXTRACE_ASSERT(this->var_pointers[2] != this->var_pointers[0]);
  }

  std::array<std::uintptr_t, 3> var_pointers;
  static inline thread_local_var<int> var{};
};

class test_thread_local_variables_can_be_large
{
public:
  auto run_thread([[maybe_unused]] int thread_index) -> void
  {
    *this->var = data{ { 1, 2, 3, 4, 5, 6, 7, 8 }, 3.14 };

    auto loaded_data = *this->var;
    CXXTRACE_ASSERT(loaded_data.integers[0] == 1);
    CXXTRACE_ASSERT(loaded_data.integers[1] == 2);
    CXXTRACE_ASSERT(loaded_data.integers[2] == 3);
    CXXTRACE_ASSERT(loaded_data.integers[3] == 4);
    CXXTRACE_ASSERT(loaded_data.integers[4] == 5);
    CXXTRACE_ASSERT(loaded_data.integers[5] == 6);
    CXXTRACE_ASSERT(loaded_data.integers[6] == 7);
    CXXTRACE_ASSERT(loaded_data.integers[7] == 8);
    CXXTRACE_ASSERT(loaded_data.d == 3.14);
  }

  auto tear_down() -> void {}

  struct data
  {
    std::array<int, 8> integers;
    double d;
  };

  thread_local_var<data> var{};
};

auto
register_concurrency_tests() -> void
{
  register_concurrency_test<test_thread_local_variable_is_zero_on_thread_entry>(
    1, concurrency_test_depth::full);
  register_concurrency_test<
    test_thread_local_variable_is_a_normal_variable_on_one_thread>(
    1, concurrency_test_depth::full);
  register_concurrency_test<test_thread_local_variables_are_thread_independent>(
    3, concurrency_test_depth::full);
  register_concurrency_test<
    test_static_thread_local_variables_are_thread_independent>(
    3, concurrency_test_depth::full);
  register_concurrency_test<test_thread_local_variables_can_be_large>(
    1, concurrency_test_depth::full);
}
}
