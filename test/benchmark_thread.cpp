#include "memory_resource.h"
#include "thread.h"
#include <benchmark/benchmark.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>

namespace cxxtrace_test {
auto
benchmark_get_current_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(cxxtrace::get_current_thread_id());
  }
}
BENCHMARK(benchmark_get_current_thread_id);

#if CXXTRACE_HAVE_MACH_THREAD
auto
benchmark_get_current_thread_mach_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(cxxtrace::get_current_thread_mach_thread_id());
  }
}
BENCHMARK(benchmark_get_current_thread_mach_thread_id);
#endif

#if CXXTRACE_HAVE_PTHREAD_THREADID_NP
auto
benchmark_get_current_thread_pthread_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(cxxtrace::get_current_thread_pthread_thread_id());
  }
}
BENCHMARK(benchmark_get_current_thread_pthread_thread_id);
#endif

auto
benchmark_fetch_and_remember_name_of_current_thread(benchmark::State& bench)
  -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = cxxtrace::detail::thread_name_set{ &allocator };
    thread_names.fetch_and_remember_name_of_current_thread(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK(benchmark_fetch_and_remember_name_of_current_thread);

#if CXXTRACE_HAVE_PROC_PIDINFO
auto
benchmark_fetch_and_remember_name_of_current_thread_libproc(
  benchmark::State& bench) -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = cxxtrace::detail::thread_name_set{ &allocator };
    thread_names.fetch_and_remember_name_of_current_thread_libproc(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK(benchmark_fetch_and_remember_name_of_current_thread_libproc);
#endif

#if CXXTRACE_HAVE_MACH_THREAD
auto
benchmark_fetch_and_remember_name_of_current_thread_mach(
  benchmark::State& bench) -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = cxxtrace::detail::thread_name_set{ &allocator };
    thread_names.fetch_and_remember_name_of_current_thread_mach(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK(benchmark_fetch_and_remember_name_of_current_thread_mach);
#endif

#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP
auto
benchmark_fetch_and_remember_name_of_current_thread_pthread(
  benchmark::State& bench) -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = cxxtrace::detail::thread_name_set{ &allocator };
    thread_names.fetch_and_remember_name_of_current_thread_pthread(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK(benchmark_fetch_and_remember_name_of_current_thread_pthread);
#endif

auto
benchmark_fetch_and_remember_name_of_current_thread_by_thread_id(
  benchmark::State& bench) -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = cxxtrace::detail::thread_name_set{ &allocator };
    thread_names.fetch_and_remember_thread_name_for_id(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK(benchmark_fetch_and_remember_name_of_current_thread_by_thread_id);

#if CXXTRACE_HAVE_PROC_PIDINFO
auto
benchmark_fetch_and_remember_name_of_current_thread_by_thread_id_libproc(
  benchmark::State& bench) -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = cxxtrace::detail::thread_name_set{ &allocator };
    thread_names.fetch_and_remember_thread_name_for_id_libproc(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK(
  benchmark_fetch_and_remember_name_of_current_thread_by_thread_id_libproc);
#endif
}
