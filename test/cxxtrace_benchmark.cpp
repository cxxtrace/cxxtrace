#include "cxxtrace_benchmark.h"

namespace cxxtrace {
namespace detail {
benchmark_group::benchmark_group(
  std::vector<benchmark::internal::Benchmark*> benchmarks)
  : benchmarks{ std::move(benchmarks) }
{}

auto
benchmark_group::Arg(std::int64_t arg) -> benchmark_group*
{
  for (auto* benchmark : this->benchmarks) {
    benchmark->Arg(arg);
  }
  return this;
}
}

benchmark_fixture::~benchmark_fixture() = default;

auto
benchmark_fixture::set_up(benchmark::State&) -> void
{
  // Do nothing.
}

auto
benchmark_fixture::tear_down(benchmark::State&) -> void
{
  // Do nothing.
}
}
