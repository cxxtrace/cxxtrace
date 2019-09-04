#include "memory_resource.h"
#include "thread.h"
#include <benchmark/benchmark.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>

using cxxtrace::detail::thread_name_set;

namespace cxxtrace_test {
namespace {
template<cxxtrace::thread_id (*GetCurrentThreadID)() noexcept>
auto
benchmark_get_current_thread_id(benchmark::State& bench) -> void
{
  for (auto _ : bench) {
    benchmark::DoNotOptimize(GetCurrentThreadID());
  }
}
BENCHMARK_TEMPLATE1(benchmark_get_current_thread_id,
                    cxxtrace::get_current_thread_id);
#if CXXTRACE_HAVE_MACH_THREAD
BENCHMARK_TEMPLATE1(benchmark_get_current_thread_id,
                    cxxtrace::get_current_thread_mach_thread_id);
#endif
#if CXXTRACE_HAVE_PTHREAD_THREADID_NP
BENCHMARK_TEMPLATE1(benchmark_get_current_thread_id,
                    cxxtrace::get_current_thread_pthread_thread_id);
#endif

template<void (thread_name_set::*FetchAndRememberNameOfCurrentThread)(
  cxxtrace::thread_id) noexcept(false)>
auto
benchmark_fetch_and_remember_name_of_current_thread(benchmark::State& bench)
  -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = thread_name_set{ &allocator };
    (thread_names.*FetchAndRememberNameOfCurrentThread)(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK_TEMPLATE1(
  benchmark_fetch_and_remember_name_of_current_thread,
  &thread_name_set::fetch_and_remember_name_of_current_thread);
#if CXXTRACE_HAVE_MACH_THREAD
BENCHMARK_TEMPLATE1(
  benchmark_fetch_and_remember_name_of_current_thread,
  &thread_name_set::fetch_and_remember_name_of_current_thread_mach);
#endif
#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP
BENCHMARK_TEMPLATE1(
  benchmark_fetch_and_remember_name_of_current_thread,
  &thread_name_set::fetch_and_remember_name_of_current_thread_pthread);
#endif

template<void (thread_name_set::*FetchAndRememberThreadNameForID)(
  cxxtrace::thread_id) noexcept(false)>
auto
benchmark_fetch_and_remember_name_of_current_thread_by_thread_id(
  benchmark::State& bench) -> void
{
  set_current_thread_name("my thread name");

  auto thread_id = cxxtrace::get_current_thread_id();
  char buffer[1024];
  for (auto _ : bench) {
    auto allocator = monotonic_buffer_resource{ buffer, sizeof(buffer) };
    auto thread_names = thread_name_set{ &allocator };
    (thread_names.*FetchAndRememberThreadNameForID)(thread_id);
    // FIXME(strager): Is this usage of DoNotOptimize correct?
    benchmark::DoNotOptimize(thread_names);
  }
}
BENCHMARK_TEMPLATE(
  benchmark_fetch_and_remember_name_of_current_thread_by_thread_id,
  &thread_name_set::fetch_and_remember_thread_name_for_id);
#if CXXTRACE_HAVE_PROC_PIDINFO
BENCHMARK_TEMPLATE(
  benchmark_fetch_and_remember_name_of_current_thread_by_thread_id,
  &thread_name_set::fetch_and_remember_thread_name_for_id_libproc);
#endif
}
}
