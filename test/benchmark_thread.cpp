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
// TODO(strager): Use the standard library's monotonic_buffer_resource
// implementation instead.
class monotonic_buffer_resource : public std::experimental::pmr::memory_resource
{
public:
  monotonic_buffer_resource(void* buffer, std::size_t buffer_size) noexcept;

  ~monotonic_buffer_resource() override;

  monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;
  monotonic_buffer_resource& operator=(const monotonic_buffer_resource&) =
    delete;

protected:
  auto do_allocate(std::size_t size, std::size_t alignment) -> void* override;
  auto do_deallocate(void*, std::size_t size, std::size_t alignment)
    -> void override;
  auto do_is_equal(const std::experimental::pmr::memory_resource&) const
    noexcept -> bool override;

private:
  void* buffer;
  std::size_t buffer_size;
  void* free_space;
};

auto set_current_thread_name(cxxtrace::czstring) -> void;

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
}

monotonic_buffer_resource::monotonic_buffer_resource(
  void* buffer,
  std::size_t buffer_size) noexcept
  : buffer{ buffer }
  , buffer_size{ buffer_size }
  , free_space{ buffer }
{}

monotonic_buffer_resource::~monotonic_buffer_resource() = default;

auto
monotonic_buffer_resource::do_allocate(std::size_t size, std::size_t alignment)
  -> void*
{
  auto free_space_size =
    this->buffer_size -
    (static_cast<std::byte*>(free_space) - static_cast<std::byte*>(buffer));
  auto* p = this->free_space;
  p = std::align(alignment, size, p, free_space_size);
  if (!p) {
    throw std::bad_alloc{};
  }
  this->free_space = static_cast<std::byte*>(p) + size;
  return p;
}

auto
monotonic_buffer_resource::do_deallocate(void*, std::size_t, std::size_t)
  -> void
{
  // Do nothing.
}

auto
monotonic_buffer_resource::do_is_equal(
  const std::experimental::pmr::memory_resource& other) const noexcept -> bool
{
  return this == &other;
}

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
