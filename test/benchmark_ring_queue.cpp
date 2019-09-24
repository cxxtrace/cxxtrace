#include "cxxtrace_benchmark.h"
#include "cxxtrace_cpp.h"
#include "mutex.h" // IWYU pragma: keep
#include "ring_queue_wrapper.h"
#include <benchmark/benchmark.h>
#include <cstdint>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/mpsc_ring_queue.h>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/spin_lock.h>
#include <cxxtrace/detail/spsc_ring_queue.h>
#include <cxxtrace/detail/warning.h>
#include <mutex>
// IWYU pragma: no_forward_declare cxxtrace_test::wrapped_mutex

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
  ring_queue_wrapper<RingQueue> queue;
};

CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(
  ring_queue_benchmark,
  (cxxtrace::detail::mpsc_ring_queue<int, 1024>),
  (cxxtrace::detail::ring_queue<int, 1024>),
  (cxxtrace::detail::spsc_ring_queue<int, 1024>));

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(ring_queue_benchmark, individual_pushes)
(benchmark::State& bench)
{
  for (auto _ : bench) {
    this->queue.reset();
#if defined(__clang__)
#pragma clang loop unroll_count(1)
#endif
    for (auto i = 0; i < this->items_per_iteration; ++i) {
      this->queue.push(
        1, [i](auto data) noexcept { data.set(0, i); });
    }
    benchmark::DoNotOptimize(this->queue);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(ring_queue_benchmark, individual_pushes)
  ->Arg(400);

template<class Mutex>
class locked_spsc_ring_queue_benchmark
  : public ring_queue_benchmark<cxxtrace::detail::spsc_ring_queue<int, 1024>>
{
protected:
  Mutex mutex;
};

// TODO(strager): Allow CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F to be called
// multiple times or something to avoid #if in macro arguments.
CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wembedded-directive")
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(
  locked_spsc_ring_queue_benchmark,
#if CXXTRACE_HAVE_OS_SPIN_LOCK
  wrapped_mutex<apple_os_spin_lock>,
#endif
#if CXXTRACE_HAVE_OS_UNFAIR_LOCK
  wrapped_mutex<apple_os_unfair_lock>,
#endif
#if defined(__x86_64__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
  wrapped_mutex<cxxtrace::detail::x86_bts_spin_lock<std::uint8_t>>,
#endif
#if defined(__x86_64__)
  wrapped_mutex<cxxtrace::detail::x86_xchg_spin_lock<std::uint8_t>>,
  wrapped_mutex<cxxtrace::detail::x86_xchg_spin_lock<std::uint16_t>>,
  wrapped_mutex<cxxtrace::detail::x86_xchg_spin_lock<std::uint32_t>>,
  wrapped_mutex<cxxtrace::detail::x86_xchg_spin_lock<std::uint64_t>>,
#endif
  wrapped_mutex<cxxtrace::detail::atomic_flag_spin_lock>,
  wrapped_mutex<cxxtrace::detail::spin_lock>,
  wrapped_mutex<compare_exchange_strong_spin_lock<std::uint16_t>>,
  wrapped_mutex<compare_exchange_strong_spin_lock<std::uint32_t>>,
  wrapped_mutex<compare_exchange_strong_spin_lock<std::uint64_t>>,
  wrapped_mutex<compare_exchange_strong_spin_lock<std::uint8_t>>,
  wrapped_mutex<std::mutex>);
CXXTRACE_WARNING_POP

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(locked_spsc_ring_queue_benchmark,
                                     individual_pushes)
(benchmark::State& bench)
{
  for (auto _ : bench) {
    this->queue.reset();
#if defined(__clang__)
#pragma clang loop unroll_count(1)
#endif
    for (auto i = 0; i < this->items_per_iteration; ++i) {
      if (this->mutex.try_lock(CXXTRACE_HERE)) {
        this->queue.push(
          1, [i](auto data) noexcept { data.set(0, i); });
        this->mutex.unlock(CXXTRACE_HERE);
      }
    }
    benchmark::DoNotOptimize(this->queue);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(locked_spsc_ring_queue_benchmark,
                                       individual_pushes)
  ->Arg(400);
}
