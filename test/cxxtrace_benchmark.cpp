#include "cxxtrace_benchmark.h"

namespace cxxtrace_test {
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

auto
benchmark_group::ArgName(cxxtrace::czstring name) -> benchmark_group*
{
  for (auto* benchmark : this->benchmarks) {
    benchmark->ArgName(name);
  }
  return this;
}

auto
benchmark_group::DenseThreadRange(int min, int max) -> benchmark_group*
{
  for (auto* benchmark : this->benchmarks) {
    benchmark->DenseThreadRange(min, max);
  }
  return this;
}

auto
benchmark_group::ThreadRange(int min, int max) -> benchmark_group*
{
  for (auto* benchmark : this->benchmarks) {
    benchmark->ThreadRange(min, max);
  }
  return this;
}

auto
benchmark_group::UseRealTime() -> benchmark_group*
{
  for (auto* benchmark : this->benchmarks) {
    benchmark->UseRealTime();
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

thread_shared_benchmark_fixture::~thread_shared_benchmark_fixture() = default;

auto
thread_shared_benchmark_fixture::set_up_thread(benchmark::State&) -> void
{
  // Do nothing.
}

auto
thread_shared_benchmark_fixture::tear_down_thread(benchmark::State&) -> void
{
  // Do nothing.
}
}
