#include "memory_resource.h"
#include <benchmark/benchmark.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <exception>
#include <experimental/memory_resource>
#include <memory>
#include <new>
#include <pthread.h>

namespace {
auto set_current_thread_name(cxxtrace::czstring) -> void;
}

namespace cxxtrace_test {
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

#if defined(__APPLE__) && defined(__MACH__)
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

#if defined(__APPLE__) && defined(__MACH__)
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

#if defined(__APPLE__) && defined(__MACH__)
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

#if defined(__APPLE__) && defined(__MACH__)
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
#endif

#if defined(__APPLE__) && defined(__MACH__)
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

namespace {
auto
set_current_thread_name(cxxtrace::czstring name) -> void
{
  auto rc = ::pthread_setname_np(name);
  if (rc != 0) {
    std::fprintf(
      stderr, "fatal: pthread_setname_np failed: %s\n", std::strerror(errno));
    std::terminate();
  }
}
}
