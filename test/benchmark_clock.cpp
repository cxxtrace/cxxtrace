#include "black_hole.h"
#include "cxxtrace_benchmark.h"
#include "cxxtrace_cpp.h"
#include <benchmark/benchmark.h>
#include <cxxtrace/clock.h>       // IWYU pragma: keep
#include <cxxtrace/clock_extra.h> // IWYU pragma: keep
#include <cxxtrace/detail/have.h> // IWYU pragma: keep

namespace cxxtrace_test {
namespace {
template<class Clock>
class clock_benchmark : public benchmark_fixture
{
public:
  using clock_type = Clock;
};
// TODO(strager): Allow CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F to be called
// multiple times or something to avoid #if in macro arguments.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wembedded-directive"
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(clock_benchmark,
#if CXXTRACE_HAVE_MACH_TIME
                                        cxxtrace::apple_absolute_time_clock,
                                        cxxtrace::apple_approximate_time_clock,
#endif
                                        cxxtrace::fake_clock,
                                        cxxtrace::posix_gettimeofday_clock,
                                        cxxtrace::std_high_resolution_clock,
                                        cxxtrace::std_steady_clock,
                                        cxxtrace::std_system_clock);
#pragma clang diagnostic pop

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(clock_benchmark, query)
(benchmark::State& bench)
{
  using clock_type = typename fixture_type::clock_type;

  auto clock = clock_type{};
  for (auto _ : bench) {
    auto sample = clock.query();
    benchmark::DoNotOptimize(sample);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(clock_benchmark, query);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(clock_benchmark, query_with_alu_workload)
(benchmark::State& bench)
{
  using clock_type = typename fixture_type::clock_type;

  auto alu_multiplier = bench.range(0);
  auto abyss = black_hole{};
  auto clock = clock_type{};
  for (auto _ : bench) {
    abyss.alu(alu_multiplier);
    auto sample = clock.query();
    benchmark::DoNotOptimize(sample);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(clock_benchmark, query_with_alu_workload)
  ->ArgName("ALU workload multiplier")
  ->Arg(1 << 0)
  ->Arg(1 << 1)
  ->Arg(1 << 2)
  ->Arg(1 << 3)
  ->Arg(1 << 4)
  ->Arg(1 << 5);
}
}
