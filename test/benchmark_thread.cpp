#include "cxxtrace_benchmark.h"
#include "memory_resource.h"
#include <benchmark/benchmark.h>
#include <cerrno>
#include <cstddef>
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

template<class ProcessorIDLookup>
class get_current_processor_id_benchmark : public benchmark_fixture
{
public:
  auto tear_down(benchmark::State& bench) -> void override
  {
    bench.counters["samples per iteration"] = this->samples_per_iteration;
    auto total_samples = bench.iterations() * this->samples_per_iteration;
    bench.counters["throughput"] = { static_cast<double>(total_samples),
                                     benchmark::Counter::kIsRate };
    bench.counters["total samples"] = total_samples;
  }

  [[gnu::always_inline]] auto get_current_processor_id() noexcept
  {
    return this->processor_id_lookup.get_current_processor_id();
  }

protected:
  static constexpr auto samples_per_iteration{ 100 };

  ProcessorIDLookup processor_id_lookup;
};

// TODO(strager): Figure out how to avoid namespace pollution while keeping the
// benchmark names short.
using namespace cxxtrace::detail;
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(
  get_current_processor_id_benchmark,
  processor_id_lookup,
  processor_id_lookup_x86_cpuid_01h,
  processor_id_lookup_x86_cpuid_0bh,
  processor_id_lookup_x86_cpuid_1fh,
  processor_id_lookup_x86_cpuid_commpage_preempt_cached);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(get_current_processor_id_benchmark,
                                     busy_loop_unrolled)
(benchmark::State& bench)
{
  for (auto _ : bench) {
#pragma clang loop unroll(full)
    for (auto i = 0; i < this->samples_per_iteration; ++i) {
      benchmark::DoNotOptimize(this->get_current_processor_id());
    }
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(get_current_processor_id_benchmark,
                                       busy_loop_unrolled)
  ->DenseThreadRange(1, benchmark::CPUInfo::Get().num_cpus * 2);
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
