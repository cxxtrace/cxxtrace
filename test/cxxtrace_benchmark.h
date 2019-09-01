#ifndef CXXTRACE_BENCHMARK_H
#define CXXTRACE_BENCHMARK_H

#include "cxxtrace_cpp.h"        // IWYU pragma: export
#include <benchmark/benchmark.h> // IWYU pragma: keep
#include <cstdint>
#include <cxxtrace/string.h>
#include <vector>

// Make CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F instantiate the given fixture
// template (deriving from cxxtrace_test::benchmark_fixture) once per given
// type.
//
// CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F must be used before using
// CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F.
#define CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(fixture_class_template, ...)   \
  static ::cxxtrace_test::detail::benchmark_template_args<CXXTRACE_CPP_MAP(    \
    CXXTRACE_BENCHMARK_GET_TYPE, __VA_ARGS__)>                                 \
    _cxxtrace_benchmark_template_args_##fixture_class_template(                \
      { CXXTRACE_CPP_STRING_LITERALS(__VA_ARGS__) })

// Like BENCHMARK_DEFINE_F, except with a template fixture.
#define CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(fixture_class_template, name)     \
  template<class _cxxtrace_benchmark_arg>                                      \
  class _cxxtrace_benchmark_class_##fixture_class_template##_##name final      \
    : public fixture_class_template<_cxxtrace_benchmark_arg>                   \
  {                                                                            \
  public:                                                                      \
    using fixture_type = fixture_class_template<_cxxtrace_benchmark_arg>;      \
                                                                               \
    auto run(::benchmark::State&) -> void;                                     \
  };                                                                           \
                                                                               \
  template<class _cxxtrace_benchmark_arg>                                      \
  void _cxxtrace_benchmark_class_##fixture_class_template##_##name<            \
    _cxxtrace_benchmark_arg>::run

// Like BENCHMARK_REGISTER_F, except with a fixture template.
//
// CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F must be used before using
// CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F.
#define CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(fixture_class_template, name)   \
  static ::cxxtrace_test::detail::benchmark_group*                             \
    _cxxtrace_benchmark_registration_##fixture_class_template##_##name =       \
      ::cxxtrace_test::detail::benchmark_register_template<                    \
        _cxxtrace_benchmark_class_##fixture_class_template##_##name>(          \
        _cxxtrace_benchmark_template_args_##fixture_class_template,            \
        #fixture_class_template,                                               \
        #name)

#define CXXTRACE_BENCHMARK_GET_TYPE(possibly_parenthesized_type)               \
  typename ::cxxtrace_test::detail::parameter_type<void(                       \
    possibly_parenthesized_type)>::type

namespace cxxtrace_test {
class benchmark_fixture
{
public:
  /* nonvirtual */ ~benchmark_fixture();

  virtual auto set_up(benchmark::State&) -> void;
  virtual auto run(benchmark::State&) -> void = 0;
  virtual auto tear_down(benchmark::State&) -> void;
};

class thread_shared_benchmark_fixture
{
public:
  /* nonvirtual */ ~thread_shared_benchmark_fixture();

  virtual auto set_up_thread(benchmark::State&) -> void;
  virtual auto run(benchmark::State&) -> void = 0;
  virtual auto tear_down_thread(benchmark::State&) -> void;
};

namespace detail {
class benchmark_group
{
public:
  explicit benchmark_group(
    std::vector<benchmark::internal::Benchmark*> benchmarks);

  auto Arg(std::int64_t) -> benchmark_group*;
  auto ArgName(cxxtrace::czstring) -> benchmark_group*;
  auto DenseThreadRange(int min, int max) -> benchmark_group*;
  auto ThreadRange(int min, int max) -> benchmark_group*;
  auto UseRealTime() -> benchmark_group*;

private:
  std::vector<benchmark::internal::Benchmark*> benchmarks;
};

template<class... Ts>
struct benchmark_template_args
{
  const char* names[sizeof...(Ts)];
};

template<template<class> class BenchmarkTemplate, class... BenchmarkArgs>
auto
benchmark_register_template(
  const benchmark_template_args<BenchmarkArgs...>& args,
  const char* fixture_name,
  const char* benchmark_name) -> benchmark_group*;
}

namespace detail {
template<class Function>
struct parameter_type;

template<class Parameter>
struct parameter_type<void(Parameter)>
{
  using type = Parameter;
};
}
}

#include "cxxtrace_benchmark_impl.h"

#endif
