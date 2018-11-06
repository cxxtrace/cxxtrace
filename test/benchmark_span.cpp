#include "cxxtrace_benchmark.h"
#include <algorithm>
#include <array>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cxxtrace/config.h>
#include <cxxtrace/ring_queue_storage.h>
#include <cxxtrace/span.h>
#include <cxxtrace/unbounded_storage.h>
#include <iostream>
#include <random>
#include <vector>

#define CXXTRACE_SPAN(category, name)                                          \
  CXXTRACE_SPAN_WITH_CONFIG(this->get_cxxtrace_config(), category, name)

namespace {
class cpu_data_cache_thrasher
{
public:
  explicit cpu_data_cache_thrasher();

  auto thrash_memory() -> void;

private:
  using index_type = int;

  static constexpr const auto memory_size = index_type{ 1 * 1024 * 1024 };
  static constexpr const auto vector_size =
    index_type{ memory_size / sizeof(int) / 2 };

  auto fill_with_random_indexes() noexcept -> void;

  auto reset_visit_counts() noexcept -> void;
  auto traverse_indexes() noexcept -> void;
  auto check_visit_counts() noexcept -> void;

  std::vector<index_type> indexes;
  std::vector<index_type> visit_counts;
};

template<class Storage>
class span_benchmark : public cxxtrace::benchmark_fixture
{
public:
  explicit span_benchmark()
    : cxxtrace_config{ cxxtrace_storage }
  {}

  auto set_up(benchmark::State& bench) -> void override
  {
    this->spans_per_iteration = bench.range(0);
    bench.counters["spans per iteration"] = this->spans_per_iteration;
  }

  auto tear_down(benchmark::State& bench) -> void override
  {
    auto total_spans = bench.iterations() * this->spans_per_iteration;
    bench.counters["total spans"] = total_spans;
    bench.counters["span throughput"] = { static_cast<double>(total_spans),
                                          benchmark::Counter::kIsRate };
  }

protected:
  auto get_cxxtrace_config() noexcept { return this->cxxtrace_config; }

  auto clear_all_samples() noexcept -> void
  {
    get_cxxtrace_config().storage().clear_all_samples();
  }

  int spans_per_iteration{ 0 };

private:
  Storage cxxtrace_storage{};
  cxxtrace::basic_config<Storage> cxxtrace_config;
};

CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(span_benchmark,
                                        cxxtrace::ring_queue_storage<1024>,
                                        cxxtrace::unbounded_storage);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(span_benchmark, enter_exit)
(benchmark::State& bench)
{
  for (auto _ : bench) {
    this->clear_all_samples();
    for (auto i = 0; i < this->spans_per_iteration; ++i) {
      auto span = CXXTRACE_SPAN("category", "span");
    }
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(span_benchmark, enter_exit)->Arg(400);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(span_benchmark, enter_enter_exit_exit)
(benchmark::State& bench)
{
  assert(this->spans_per_iteration % 2 == 0);
  auto inner_iteration_count = this->spans_per_iteration / 2;
  for (auto _ : bench) {
    this->clear_all_samples();
    for (auto i = 0; i < inner_iteration_count; ++i) {
      auto outer_span = CXXTRACE_SPAN("category", "outer span");
      auto inner_span = CXXTRACE_SPAN("category", "inner span");
    }
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(span_benchmark, enter_enter_exit_exit)
  ->Arg(400);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(span_benchmark, enter_thrash_memory_exit)
(benchmark::State& bench)
{
  auto thrasher = cpu_data_cache_thrasher{};
  for (auto _ : bench) {
    this->clear_all_samples();
    for (auto i = 0; i < this->spans_per_iteration; ++i) {
      auto span = CXXTRACE_SPAN("category", "thrash memory");
      thrasher.thrash_memory();
    }
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(span_benchmark, enter_thrash_memory_exit)
  ->Arg(400);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(span_benchmark, thrash_memory)
(benchmark::State& bench)
{
  auto thrasher = cpu_data_cache_thrasher{};
  for (auto _ : bench) {
    this->clear_all_samples();
    for (auto i = 0; i < this->spans_per_iteration; ++i) {
      thrasher.thrash_memory();
    }
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(span_benchmark, thrash_memory)->Arg(400);

cpu_data_cache_thrasher::cpu_data_cache_thrasher()
{
  this->indexes.resize(this->vector_size);
  this->visit_counts.resize(this->vector_size);

  this->fill_with_random_indexes();
}

auto
cpu_data_cache_thrasher::thrash_memory() -> void
{
  this->reset_visit_counts();
  this->traverse_indexes();
  this->check_visit_counts();
}

auto
cpu_data_cache_thrasher::fill_with_random_indexes() noexcept -> void
{
  assert(this->indexes.size() == this->vector_size);

  std::iota(this->indexes.begin(), this->indexes.end(), index_type{ 0 });
  std::shuffle(this->indexes.begin(), this->indexes.end(), std::mt19937{});
}

auto
cpu_data_cache_thrasher::reset_visit_counts() noexcept -> void
{
  std::fill(
    this->visit_counts.begin(), this->visit_counts.end(), index_type{ 0 });
}

auto
cpu_data_cache_thrasher::traverse_indexes() noexcept -> void
{
  assert(this->indexes.size() == this->vector_size);
  assert(this->visit_counts.size() == this->vector_size);

  constexpr auto parallelism = index_type{ 8 };
  assert(parallelism <= this->vector_size);
  assert(this->vector_size % parallelism == 0);

  auto cur_indexes = std::array<index_type, parallelism>{};
  std::copy_n(
    this->visit_counts.begin(), cur_indexes.size(), cur_indexes.begin());
  for (auto i = index_type{ 0 }; i < this->vector_size / parallelism; ++i) {
    for (auto& cur_index : cur_indexes) {
      this->visit_counts[cur_index] += 1;
      cur_index = this->indexes[cur_index];
    }
  }
}

auto
cpu_data_cache_thrasher::check_visit_counts() noexcept -> void
{
  auto encountered_problem = false;
  auto total_visits = index_type{ 0 };
  for (auto i = index_type{ 0 }; i < this->vector_size; ++i) {
    auto visit_count = this->visit_counts[i];
    if (visit_count > 10) {
      std::cerr << "error: Index " << i << " visited too many times ("
                << visit_count << " times)\n";
      encountered_problem = true;
    }
    total_visits += visit_count;
  }
  assert(!encountered_problem);
  benchmark::DoNotOptimize(encountered_problem);
  assert(total_visits == this->vector_size);
  benchmark::DoNotOptimize(total_visits);
}
}
