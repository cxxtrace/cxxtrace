#ifndef CXXTRACE_TEST_SPAN_H
#define CXXTRACE_TEST_SPAN_H

#include <cstddef>
#include <cxxtrace/clock.h>
#include <cxxtrace/config.h>
#include <cxxtrace/mpmc_ring_queue_processor_local_storage.h>
#include <cxxtrace/mpmc_ring_queue_storage.h>
#include <cxxtrace/ring_queue_storage.h>
#include <cxxtrace/ring_queue_thread_local_storage.h>
#include <cxxtrace/ring_queue_unsafe_storage.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/spmc_ring_queue_thread_local_storage.h>
#include <cxxtrace/unbounded_storage.h>
#include <cxxtrace/unbounded_unsafe_storage.h>

namespace cxxtrace_test {
struct ring_queue_thread_local_test_storage_tag
{};
template<std::size_t CapacityPerThread, class ClockSample>
using ring_queue_thread_local_test_storage =
  cxxtrace::ring_queue_thread_local_storage<
    CapacityPerThread,
    ring_queue_thread_local_test_storage_tag,
    ClockSample>;

struct spmc_ring_queue_thread_local_test_storage_tag
{};
template<std::size_t CapacityPerThread, class ClockSample>
using spmc_ring_queue_thread_local_test_storage =
  cxxtrace::spmc_ring_queue_thread_local_storage<
    CapacityPerThread,
    spmc_ring_queue_thread_local_test_storage_tag,
    ClockSample>;

using clock = cxxtrace::fake_clock;
using clock_sample = clock::sample;

template<class Storage>
class test_span : public testing::Test
{
public:
  explicit test_span()
    : cxxtrace_config{ cxxtrace_storage, clock_ }
  {}

  auto TearDown() -> void override { this->reset_storage(); }

protected:
  using clock_type = clock;
  using storage_type = Storage;

  auto get_cxxtrace_config() noexcept
    -> cxxtrace::basic_config<storage_type, clock_type>&
  {
    return this->cxxtrace_config;
  }

  auto take_all_samples() -> cxxtrace::samples_snapshot
  {
    return this->cxxtrace_storage.take_all_samples(this->clock());
  }

  auto reset_storage() -> void { this->cxxtrace_storage.reset(); }

  auto clock() noexcept -> clock_type& { return this->clock_; }

private:
  clock_type clock_{};
  storage_type cxxtrace_storage{};
  cxxtrace::basic_config<storage_type, clock_type> cxxtrace_config;
};

using test_span_types = ::testing::Types<
  cxxtrace::mpmc_ring_queue_processor_local_storage<1024, clock_sample>,
  cxxtrace::mpmc_ring_queue_storage<1024, clock_sample>,
  cxxtrace::ring_queue_storage<1024, clock_sample>,
  cxxtrace::ring_queue_unsafe_storage<1024, clock_sample>,
  cxxtrace::unbounded_storage<clock_sample>,
  cxxtrace::unbounded_unsafe_storage<clock_sample>,
  ring_queue_thread_local_test_storage<1024, clock_sample>,
  spmc_ring_queue_thread_local_test_storage<1024, clock_sample>>;
TYPED_TEST_CASE(test_span, test_span_types, );

template<class Storage>
class test_span_thread_safe : public test_span<Storage>
{};

using test_span_thread_safe_types = ::testing::Types<
  cxxtrace::mpmc_ring_queue_processor_local_storage<1024, clock_sample>,
  cxxtrace::mpmc_ring_queue_storage<1024, clock_sample>,
  cxxtrace::ring_queue_storage<1024, clock_sample>,
  cxxtrace::unbounded_storage<clock_sample>,
  ring_queue_thread_local_test_storage<1024, clock_sample>,
  spmc_ring_queue_thread_local_test_storage<1024, clock_sample>>;
TYPED_TEST_CASE(test_span_thread_safe, test_span_thread_safe_types, );
}

#endif
