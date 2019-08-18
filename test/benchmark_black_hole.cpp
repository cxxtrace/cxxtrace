#include "black_hole.h"
#include <benchmark/benchmark.h>

namespace cxxtrace_test {
auto
black_hole_benchmark_alu(benchmark::State& bench) -> void
{
  auto alu_multiplier = bench.range(0);
  auto abyss = black_hole{};
  for (auto _ : bench) {
    abyss.alu(alu_multiplier);
  }
}
BENCHMARK(black_hole_benchmark_alu)
  ->ArgName("ALU workload multiplier")
  ->Arg(1 << 0)
  ->Arg(1 << 1)
  ->Arg(1 << 2)
  ->Arg(1 << 3)
  ->Arg(1 << 4)
  ->Arg(1 << 5);
}
