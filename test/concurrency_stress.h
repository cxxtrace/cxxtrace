#ifndef CXXTRACE_TEST_CONCURRENCY_STRESS_H
#define CXXTRACE_TEST_CONCURRENCY_STRESS_H

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include <cstdint>
#endif

namespace cxxtrace_test {
#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
using concurrency_stress_generation = std::int64_t;

auto
current_concurrency_stress_generation() noexcept
  -> concurrency_stress_generation;
#endif
}

#endif
