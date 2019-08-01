#include "cxxtrace_benchmark.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <cxxtrace/clock.h>
#include <cxxtrace/clock_extra.h>

namespace cxxtrace_test {
namespace {
auto
perform_alu_work() noexcept -> void;

template<class Clock>
class clock_benchmark : public benchmark_fixture
{
public:
  using clock_type = Clock;
};
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(clock_benchmark,
                                        cxxtrace::apple_absolute_time_clock,
                                        cxxtrace::apple_approximate_time_clock,
                                        cxxtrace::fake_clock,
                                        cxxtrace::posix_gettimeofday_clock,
                                        cxxtrace::std_high_resolution_clock,
                                        cxxtrace::std_steady_clock,
                                        cxxtrace::std_system_clock);

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

  auto clock = clock_type{};
  for (auto _ : bench) {
    perform_alu_work();
    auto sample = clock.query();
    benchmark::DoNotOptimize(sample);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(clock_benchmark,
                                       query_with_alu_workload);

auto
clock_benchmark_alu_workload_baseline(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    perform_alu_work();
  }
}
BENCHMARK(clock_benchmark_alu_workload_baseline);

auto
perform_alu_work() noexcept -> void
{
  auto iterations = 3;
  auto theta = 0.42f;
  for (auto i = 0; i < iterations; ++i) {
    benchmark::DoNotOptimize(theta);
    theta += 0.13f;
    auto result = std::sinf(theta);
    benchmark::DoNotOptimize(result);
  }
}
}
}
