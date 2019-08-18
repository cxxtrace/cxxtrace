#include "black_hole.h"
#include "cxxtrace_benchmark.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <cassert>
#include <cstdio>
#include <cxxtrace/detail/fickle_lock.h>
#include <exception>
#include <mutex>
#include <utility>

namespace {
template<class Mutex>
class wrapped_mutex
{
public:
  template<class Func>
  [[gnu::always_inline]] auto try_with_lock(
    cxxtrace::thread_id,
    Func&& critical_section,
    cxxtrace::detail::debug_source_location) -> bool
  {
    auto guard = std::lock_guard<Mutex>{ this->underlying_mutex };
    std::forward<Func>(critical_section)();
    return true;
  }

  Mutex underlying_mutex;
};

class exchange_spin_lock
{
public:
  auto lock() -> void
  {
  retry:
    auto was_locked = this->is_locked.exchange(true, std::memory_order_acquire);
    if (was_locked) {
      goto retry;
    }
  }

  auto unlock() -> void
  {
    this->is_locked.store(false, std::memory_order_release);
  }

private:
  std::atomic<bool> is_locked;
};
}

namespace cxxtrace_test {
template<class Mutex>
class mutex_benchmark : public benchmark_fixture
{
public:
  auto set_up(benchmark::State&) -> void override
  {
    this->assert_mutex_can_be_locked(cxxtrace::get_current_thread_id());
  }

protected:
  auto assert_mutex_can_be_locked(cxxtrace::thread_id current_thread_id) -> void
  {
    auto acquired =
      this->mutex.try_with_lock(current_thread_id, [] {}, CXXTRACE_HERE);
    assert(acquired);
    if (!acquired) {
      std::fprintf(stderr, "fatal: try_with_lock failed\n");
      std::terminate();
    }
  }

  Mutex mutex;
};
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(mutex_benchmark,
                                        cxxtrace::detail::fickle_lock,
                                        wrapped_mutex<exchange_spin_lock>,
                                        wrapped_mutex<std::mutex>);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(mutex_benchmark, uncontended_empty)
(benchmark::State& bench)
{
  auto thread_id = cxxtrace::get_current_thread_id();
  for (auto _ : bench) {
    this->mutex.try_with_lock(thread_id, [] {}, CXXTRACE_HERE);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(mutex_benchmark, uncontended_empty);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(mutex_benchmark, uncontended_with_alu_work)
(benchmark::State& bench)
{
  auto thread_id = cxxtrace::get_current_thread_id();
  auto abyss = black_hole{};
  auto alu_multiplier = bench.range(0);
  for (auto _ : bench) {
    this->mutex.try_with_lock(
      thread_id, [&] { abyss.alu(alu_multiplier); }, CXXTRACE_HERE);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(mutex_benchmark,
                                       uncontended_with_alu_work)
  ->Arg(1 << 0)
  ->Arg(1 << 1)
  ->Arg(1 << 2)
  ->Arg(1 << 3)
  ->Arg(1 << 4)
  ->ArgName("ALU work multiplier");

template<class Mutex>
class biased_mutex_benchmark : public mutex_benchmark<Mutex>
{};
CXXTRACE_BENCHMARK_CONFIGURE_TEMPLATE_F(biased_mutex_benchmark,
                                        cxxtrace::detail::fickle_lock);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(biased_mutex_benchmark,
                                     same_thread_uncontended_empty)
(benchmark::State& bench)
{
  auto thread_id_1 = cxxtrace::get_current_thread_id();
  auto thread_id_2 = cxxtrace::get_current_thread_id();
  this->assert_mutex_can_be_locked(thread_id_1);
  this->assert_mutex_can_be_locked(thread_id_2);
  this->assert_mutex_can_be_locked(thread_id_1);
  for (auto _ : bench) {
    this->mutex.try_with_lock(thread_id_1, [] {}, CXXTRACE_HERE);
    this->mutex.try_with_lock(thread_id_2, [] {}, CXXTRACE_HERE);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(biased_mutex_benchmark,
                                       same_thread_uncontended_empty);

CXXTRACE_BENCHMARK_DEFINE_TEMPLATE_F(biased_mutex_benchmark,
                                     alternating_thread_uncontended_empty)
(benchmark::State& bench)
{
  auto thread_id_1 = cxxtrace::get_current_thread_id();
  auto thread_id_2 = cxxtrace::get_current_thread_id() + 1;
  this->assert_mutex_can_be_locked(thread_id_1);
  this->assert_mutex_can_be_locked(thread_id_2);
  this->assert_mutex_can_be_locked(thread_id_1);
  for (auto _ : bench) {
    this->mutex.try_with_lock(thread_id_1, [] {}, CXXTRACE_HERE);
    this->mutex.try_with_lock(thread_id_2, [] {}, CXXTRACE_HERE);
  }
}
CXXTRACE_BENCHMARK_REGISTER_TEMPLATE_F(biased_mutex_benchmark,
                                       alternating_thread_uncontended_empty);
}
