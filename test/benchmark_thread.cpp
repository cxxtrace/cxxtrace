#include <benchmark/benchmark.h>
#include <cxxtrace/thread.h>

namespace {
auto
benchmark_get_current_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(cxxtrace::get_current_thread_id());
  }
}
BENCHMARK(benchmark_get_current_thread_id);

#if defined(__APPLE__) && defined(__MACH__)
auto
benchmark_get_current_thread_mach_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(cxxtrace::get_current_thread_mach_thread_id());
  }
}
BENCHMARK(benchmark_get_current_thread_mach_thread_id);

auto
benchmark_get_current_thread_pthread_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(cxxtrace::get_current_thread_pthread_thread_id());
  }
}
BENCHMARK(benchmark_get_current_thread_pthread_thread_id);
#endif
}
