#ifndef CXXTRACE_BENCHMARK_IMPL_H
#define CXXTRACE_BENCHMARK_IMPL_H

#if !defined(CXXTRACE_BENCHMARK_H)
#error                                                                         \
  "Include \"cxxtrace_benchmark.h\" instead of including \"cxxtrace_benchmark_impl.h\" directly."
#endif

#include <benchmark/benchmark.h>
#include <string>
#include <utility>
#include <vector>

namespace cxxtrace {
namespace detail {
template<class Benchmark>
auto
benchmark_register_template_instantiation(const char* fixture_name,
                                          const char* benchmark_arg_name,
                                          const char* benchmark_name)
  -> benchmark::internal::Benchmark*;

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
    auto benchmark = Benchmark{};
    benchmark.set_up(bench);
    benchmark.run(bench);
    benchmark.tear_down(bench);
  };
  return benchmark::RegisterBenchmark(full_name.c_str(), benchmark_function);
}
}
}

#endif
