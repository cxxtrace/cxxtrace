#include "cxxtrace_benchmark.h"
#include "ring_queue.h"
#include <benchmark/benchmark.h>
#include <cxxtrace/detail/mpmc_ring_queue.h>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/spmc_ring_queue.h>

namespace cxxtrace_test {
template<class RingQueue>
class ring_queue_benchmark : public benchmark_fixture
{
public:
  auto set_up(benchmark::State& bench) -> void override
  {
    this->items_per_iteration = bench.range(0);
    bench.counters["items per iteration"] = this->items_per_iteration;
  }

  auto tear_down(benchmark::State& bench) -> void override
  {
    auto total_items = bench.iterations() * this->items_per_iteration;
    bench.counters["total items"] = total_items;
    bench.counters["item throughput"] = { static_cast<double>(total_items),
                                          benchmark::Counter::kIsRate };
  }

protected:
  int items_per_iteration /*uninitialized*/;
  RingQueue queue;
};

CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(
  ring_queue_benchmark,
  (cxxtrace::detail::mpmc_ring_queue<int, 1024>),
  (cxxtrace::detail::ring_queue<int, 1024>),
  (cxxtrace::detail::spmc_ring_queue<int, 1024>));

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(ring_queue_benchmark, individual_pushes)
(benchmark::State& bench)
{
  for (auto _ : bench) {
    this->queue.reset();
    for (auto i = 0; i < this->items_per_iteration; ++i) {
      push(this->queue, 1, [i](auto data) noexcept { data.set(0, i); });
    }
    benchmark::DoNotOptimize(this->queue);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(ring_queue_benchmark, individual_pushes)
  ->Arg(400);
}
