#include "exhaustive_rng.h"
#include "reference_ring_queue.h"
#include "ring_queue.h"
#include "stringify.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cxxtrace/detail/mpmc_ring_queue.h>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/spmc_ring_queue.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <signal.h>
#include <type_traits>
#include <vector>

#define RING_QUEUE typename TestFixture::template ring_queue

using testing::ContainerEq;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace {
struct point
{
  double x;
  double y;
};
}

namespace cxxtrace {
namespace detail {
// Ensure all methods are instantiatable.
template class ring_queue<int, 1, int>;
template class ring_queue<char, 1, signed char>;
template class ring_queue<point, 1024, std::size_t>;

template class spmc_ring_queue<int, 1, int>;
template class spmc_ring_queue<char, 1, signed char>;
template class spmc_ring_queue<point, 1024, std::size_t>;
}
}

namespace cxxtrace_test {
template<class RingQueueFactory>
class test_ring_queue : public testing::Test
{
public:
  template<class T, std::size_t Capacity, class Index = int>
  using ring_queue =
    typename RingQueueFactory::template ring_queue<T, Capacity, Index>;
};

template<template<class T, std::size_t Capacity, class Index> class RingQueue>
struct ring_queue_factory
{
  template<class T, std::size_t Capacity, class Index>
  using ring_queue = RingQueue<T, Capacity, Index>;
};

using test_ring_queue_types =
  ::testing::Types<ring_queue_factory<cxxtrace::detail::ring_queue>,
                   ring_queue_factory<cxxtrace::detail::mpmc_ring_queue>,
                   ring_queue_factory<cxxtrace::detail::spmc_ring_queue>>;
TYPED_TEST_CASE(test_ring_queue, test_ring_queue_types, );

namespace {
template<class RingQueue>
auto
pop_all(RingQueue&) -> std::vector<typename RingQueue::value_type>;
}

TYPED_TEST(test_ring_queue, new_queue_is_empty)
{
  auto queue = RING_QUEUE<int, 64>{};
  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  EXPECT_THAT(contents, IsEmpty());
}

TYPED_TEST(test_ring_queue, pop_returns_single_pushed_int)
{
  auto queue = RING_QUEUE<int, 64>{};
  push(queue, 1, [](auto data) noexcept->void { data.set(0, 42); });

  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  ASSERT_THAT(contents, ElementsAre(42));
}

TYPED_TEST(test_ring_queue, pop_returns_many_individually_pushed_ints)
{
  auto queue = RING_QUEUE<int, 64>{};
  auto values_to_write = std::vector<int>{ 42, 9001, -1 };
  for (auto value : values_to_write) {
    push(queue, 1, [value](auto data) noexcept->void { data.set(0, value); });
  }

  auto written_values = pop_all(queue);
  EXPECT_THAT(written_values, ContainerEq(values_to_write));
}

TYPED_TEST(test_ring_queue, pop_returns_all_bulk_pushed_ints)
{
  auto queue = RING_QUEUE<int, 64>{};
  push(queue, 3, [](auto data) noexcept->void {
    data.set(0, 42);
    data.set(1, 9001);
    data.set(2, -1);
  });

  auto written_values = pop_all(queue);
  EXPECT_THAT(written_values, ElementsAre(42, 9001, -1));
}

TYPED_TEST(test_ring_queue, bulk_pushing_at_end_of_ring_preserves_all_items)
{
  auto queue = RING_QUEUE<int, 8>{};
  push(queue, 6, [](auto) noexcept->void{});
  pop_all(queue);

  push(queue, 4, [](auto data) noexcept->void {
    data.set(0, 10);
    data.set(1, 20);
    data.set(2, 30);
    data.set(3, 40);
  });

  auto written_values = pop_all(queue);
  EXPECT_THAT(written_values, ElementsAre(10, 20, 30, 40));
}

TYPED_TEST(test_ring_queue, popping_again_returns_no_items)
{
  auto queue = RING_QUEUE<int, 64>{};
  push(queue, 5, [](auto data) noexcept->void {
    data.set(0, 10);
    data.set(1, 20);
    data.set(2, 30);
    data.set(3, 40);
    data.set(4, 50);
  });

  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  contents.clear();
  queue.pop_all_into(contents);
  EXPECT_THAT(contents, IsEmpty());
}

TYPED_TEST(test_ring_queue, reset_then_pop_returns_no_items)
{
  auto queue = RING_QUEUE<int, 64>{};
  push(queue, 5, [](auto data) noexcept->void {
    data.set(0, 10);
    data.set(1, 20);
    data.set(2, 30);
    data.set(3, 40);
    data.set(4, 50);
  });

  queue.reset();
  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  EXPECT_THAT(contents, IsEmpty());
}

TYPED_TEST(test_ring_queue, overflow_causes_pop_to_return_only_newest_data)
{
  auto queue = RING_QUEUE<int, 4>{};
  for (auto value : { 10, 20, 30, 40, 50 }) {
    push(queue, 1, [value](auto data) noexcept->void { data.set(0, value); });
  }

  auto items = pop_all(queue);
  EXPECT_THAT(items, ElementsAre(20, 30, 40, 50));
}

template<class RingQueue>
class test_ring_queue_against_reference : public testing::Test
{
protected:
  using size_type = typename RingQueue::size_type;
  using value_type = typename RingQueue::value_type;

  static_assert(std::is_same_v<value_type, int>);

  static constexpr auto capacity = RingQueue::capacity;

  auto reset() -> void
  {
    this->queue.reset();
    this->reference_queue.reset();
  }

  auto push_n(int count) -> void
  {
    for (auto i = 0; i < count; ++i) {
      push(this->queue, 1, [this](auto data) noexcept->void {
        data.set(0, this->cur_value);
      });
      this->reference_queue.push(this->cur_value);
      this->cur_value += 1;
    }
  }

  auto pop_all_and_check(int max_size) -> void
  {
    auto popped = pop_all(this->queue);
    auto reference_popped = this->reference_queue.pop_n_and_reset(max_size);
    EXPECT_THAT(popped, ContainerEq(reference_popped));
  }

  RingQueue queue{};
  reference_ring_queue<value_type, capacity> reference_queue{};
  value_type cur_value{ 100 };
};

using test_ring_queue_against_reference_types =
  ::testing::Types<cxxtrace::detail::ring_queue<int, 8, int>,
                   cxxtrace::detail::spmc_ring_queue<int, 8, int>>;
TYPED_TEST_CASE(test_ring_queue_against_reference,
                test_ring_queue_against_reference_types, );

TYPED_TEST(test_ring_queue_against_reference,
           overflow_causes_pop_to_return_only_newest_data_exhaustive)
{
  auto rng = exhaustive_rng{};
  while (!rng.done() && !this->HasFailure()) {
    auto reader_offset = rng.next_integer_0(this->capacity * 5);
    SCOPED_TRACE(stringify("reader_offset = ", reader_offset));
    auto writer_lead = rng.next_integer_0(this->capacity * 5);
    SCOPED_TRACE(stringify("writer_lead = ", writer_lead));

    this->push_n(reader_offset);
    this->reset();

    this->push_n(writer_lead);
    this->pop_all_and_check(this->capacity);

    rng.lap();
    this->reset();
  }
}

TYPED_TEST(test_ring_queue, overflowing_size_type_is_not_supported_yet)
{
  // TODO(strager): Instead of asserting, make overflowing the size type not
  // special at all.
  auto queue = RING_QUEUE<int, 8, signed char>{};
  auto max_index =
    std::numeric_limits<typename decltype(queue)::size_type>::max();
  for (auto i = 0; i < max_index; ++i) {
    push(queue, 1, [](auto data) noexcept { data.set(0, 0); });
  }
  EXPECT_EXIT({ push(queue, 1, [](auto data) noexcept { data.set(0, 0); }); },
              testing::KilledBySignal(SIGABRT),
              "Writer overflowed size_type");
}

namespace {
template<class RingQueue>
auto
pop_all(RingQueue& queue) -> std::vector<typename RingQueue::value_type>
{
  auto data = std::vector<typename RingQueue::value_type>{};
  queue.pop_all_into(data);
  return data;
}
}
}
