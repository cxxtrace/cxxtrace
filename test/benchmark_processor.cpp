#include "cxxtrace_benchmark.h"
#include <benchmark/benchmark.h>
#include <cxxtrace/detail/processor.h>

namespace cxxtrace_test {
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
  processor_id_lookup_x86_cpuid_uncached,
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
