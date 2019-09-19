#include "cxxtrace_benchmark.h"
#include "cxxtrace_cpp.h"
#include <benchmark/benchmark.h>
#include <cxxtrace/detail/have.h> // IWYU pragma: keep
#include <cxxtrace/detail/processor.h>

namespace cxxtrace_test {
template<class ProcessorIDLookup>
class get_current_processor_id_benchmark : public benchmark_fixture
{
public:
  using lookup = ProcessorIDLookup;
  using lookup_thread_local_cache = typename lookup::thread_local_cache;

  auto tear_down(benchmark::State& bench) -> void override
  {
    bench.counters["samples per iteration"] = this->samples_per_iteration;
    auto total_samples = bench.iterations() * this->samples_per_iteration;
    bench.counters["throughput"] = { static_cast<double>(total_samples),
                                     benchmark::Counter::kIsRate };
    bench.counters["total samples"] = total_samples;
  }

  [[gnu::always_inline]] auto get_current_processor_id(
    lookup_thread_local_cache& cache) noexcept
  {
    return this->processor_id_lookup.get_current_processor_id(cache);
  }

protected:
  static constexpr auto samples_per_iteration{ 100 };

  lookup processor_id_lookup;
};

// TODO(strager): Figure out how to avoid namespace pollution while keeping the
// benchmark names short.
using namespace cxxtrace::detail;
// TODO(strager): Allow CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F to be called
// multiple times or something to avoid #if in macro arguments.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wembedded-directive"
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(
  get_current_processor_id_benchmark,
#if CXXTRACE_HAVE_APPLE_COMMPAGE
  processor_id_lookup_x86_cpuid_commpage_preempt_cached,
#endif
  processor_id_lookup,
#if CXXTRACE_HAVE_SCHED_GETCPU
  processor_id_lookup_sched_getcpu,
#endif
#if CXXTRACE_HAVE_RSEQ
  processor_id_lookup_rseq,
#endif
  processor_id_lookup_x86_cpuid_01h,
  processor_id_lookup_x86_cpuid_0bh,
  processor_id_lookup_x86_cpuid_1fh,
#if defined(__x86_64__) && CXXTRACE_HAVE_PROCESSOR_ID_IN_X86_TSC_AUX
  processor_id_lookup_x86_rdpid,
  processor_id_lookup_x86_rdtscp,
#endif
  processor_id_lookup_x86_cpuid_uncached);
#pragma clang diagnostic pop

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(get_current_processor_id_benchmark,
                                     busy_loop_unrolled)
(benchmark::State& bench)
{
  if (!this->processor_id_lookup.supported()) {
    bench.SkipWithError("This processor ID lookup method is not supported");
    for (auto _ : bench) {
    }
    return;
  }

  using lookup_thread_local_cache =
    typename fixture_type::lookup_thread_local_cache;
  auto cache = lookup_thread_local_cache{ this->processor_id_lookup };
  for (auto _ : bench) {
    // TODO(strager): GCC does weird things to this loop for
    // processor_id_lookup_x86_rdtscp (with #pragma GCC unroll). GCC behaves as
    // if samples_per_iteration == 1. Hypothesis: GCC thinks that two adjacent
    // rdtscp instructions can return the same value; rdtscp is
    // __attribute__((pure)). Figure out how to fix GCC's code generation to
    // make benchmark comparisons more fair.
#if defined(__clang__)
#pragma clang loop unroll(full)
#elif defined(__GNUC__)
#pragma GCC unroll 65534
#endif
    for (auto i = 0; i < this->samples_per_iteration; ++i) {
      benchmark::DoNotOptimize(this->get_current_processor_id(cache));
    }
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(get_current_processor_id_benchmark,
                                       busy_loop_unrolled)
  ->DenseThreadRange(1, benchmark::CPUInfo::Get().num_cpus * 2);
}
