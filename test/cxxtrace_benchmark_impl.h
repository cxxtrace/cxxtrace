#ifndef CXXTRACE_BENCHMARK_IMPL_H
#define CXXTRACE_BENCHMARK_IMPL_H

#if !defined(CXXTRACE_BENCHMARK_H)
#error                                                                         \
  "Include \"cxxtrace_benchmark.h\" instead of including \"cxxtrace_benchmark_impl.h\" directly."
#endif

#include <benchmark/benchmark.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <optional>

namespace cxxtrace {
namespace detail {
template<class Benchmark>
auto
benchmark_register_template_instantiation(const char* fixture_name,
                                          const char* benchmark_arg_name,
                                          const char* benchmark_name)
  -> benchmark::internal::Benchmark*;

template<class Benchmark, class Enable = void>
class benchmark_runner;

template<class Benchmark>
class benchmark_runner<
  Benchmark,
  std::enable_if_t<std::is_base_of_v<benchmark_fixture, Benchmark>>>
{
public:
  static auto run(benchmark::State& bench) -> void
  {
    auto benchmark = Benchmark{};
    benchmark.set_up(bench);
    benchmark.run(bench);
    benchmark.tear_down(bench);
  }
};

class thread_shared_benchmark_runner_base
{
protected:
  static inline std::mutex mutex{};
  static inline std::condition_variable cond_var{};
  static inline int exited_threads{ 0 };
};

template<class Benchmark>
class benchmark_runner<
  Benchmark,
  std::enable_if_t<
    std::is_base_of_v<thread_shared_benchmark_fixture, Benchmark>>>
  : private thread_shared_benchmark_runner_base
{
public:
  static auto run(benchmark::State& bench) -> void
  {
    auto runner = benchmark_runner{ bench };
    auto& benchmark = runner.shared_benchmark();
    benchmark.set_up_thread(bench);
    benchmark.run(bench);
    benchmark.tear_down_thread(bench);
  }

  explicit benchmark_runner(const benchmark::State& bench) noexcept(false)
    : thread_count{ bench.threads }
    , is_main_thread{ bench.thread_index == 0 }
  {
    this->construct_benchmark_if_main_thread();
    this->enter_benchmark();
  }

  benchmark_runner(const benchmark_runner&) = delete;
  benchmark_runner& operator=(const benchmark_runner&) = delete;
  benchmark_runner(benchmark_runner&&) = delete;
  benchmark_runner& operator=(benchmark_runner&&) = delete;

  ~benchmark_runner() noexcept(false)
  {
    this->exit_benchmark();
    this->destruct_benchmark_if_main_thread();
  }

  auto shared_benchmark() noexcept -> Benchmark& { return *this->benchmark; }

private:
  auto construct_benchmark_if_main_thread() -> void
  {
    if (this->is_main_thread) {
      this->construct_benchmark();
    }
  }

  auto construct_benchmark() -> void
  {
    auto lock = std::lock_guard{ this->mutex };
    assert(!this->benchmark.has_value());
    assert(this->exited_threads == 0);
    this->benchmark.emplace();
    this->cond_var.notify_all();
  }

  auto destruct_benchmark_if_main_thread() -> void
  {
    if (this->is_main_thread) {
      this->destruct_benchmark();
    }
  }

  auto destruct_benchmark() -> void
  {
    auto lock = std::unique_lock{ this->mutex };
    assert(this->benchmark.has_value());
    this->cond_var.wait(
      lock, [&] { return this->exited_threads >= this->thread_count; });
    assert(this->exited_threads == this->thread_count);
    this->exited_threads = 0;
    this->benchmark = std::nullopt;
  }

  auto enter_benchmark() -> void
  {
    auto lock = std::unique_lock{ this->mutex };
    this->cond_var.wait(lock, [&] { return this->benchmark.has_value(); });
  }

  auto exit_benchmark() -> void
  {
    auto lock = std::lock_guard{ this->mutex };
    this->exited_threads += 1;
    this->cond_var.notify_all();
  }

  static inline std::optional<Benchmark> benchmark{};

  int thread_count;
  bool is_main_thread;
};

template<template<class> class BenchmarkTemplate, class... BenchmarkArgs>
auto
benchmark_register_template(
  const benchmark_template_args<BenchmarkArgs...>& args,
  const char* fixture_name,
  const char* benchmark_name) -> benchmark_group*
{
  using arg_indexes = std::index_sequence_for<BenchmarkArgs...>;
  return benchmark_register_template<BenchmarkTemplate, BenchmarkArgs...>(
    args, arg_indexes{}, fixture_name, benchmark_name);
}

template<template<class> class BenchmarkTemplate,
         class... BenchmarkArgs,
         std::size_t... BenchmarkArgIndexes>
auto
benchmark_register_template(
  const benchmark_template_args<BenchmarkArgs...>& args,
  std::index_sequence<BenchmarkArgIndexes...>,
  const char* fixture_name,
  const char* benchmark_name) -> benchmark_group*
{
  return new benchmark_group{ { benchmark_register_template_instantiation<
    BenchmarkTemplate<BenchmarkArgs>>(
    fixture_name, args.names[BenchmarkArgIndexes], benchmark_name)... } };
}

template<class Benchmark>
auto
benchmark_register_template_instantiation(const char* fixture_name,
                                          const char* benchmark_arg_name,
                                          const char* benchmark_name)
  -> benchmark::internal::Benchmark*
{
  using namespace std::literals::string_literals;

  auto full_name =
    fixture_name + "<"s + benchmark_arg_name + ">/"s + benchmark_name;
  auto benchmark_function = [](benchmark::State& bench) {
    return benchmark_runner<Benchmark>::run(bench);
  };
  return benchmark::RegisterBenchmark(full_name.c_str(), benchmark_function);
}
}
}

#endif
