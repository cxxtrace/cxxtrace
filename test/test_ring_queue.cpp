#include "exhaustive_rng.h"
#include "reference_ring_queue.h"
#include "stringify.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cxxtrace/detail/ring_queue.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <signal.h>
#include <type_traits>
#include <vector>

using cxxtrace::stringify;
using cxxtrace::detail::ring_queue;
using testing::ContainerEq;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace cxxtrace {
namespace detail {
// Ensure all methods are instantiatable.
template class ring_queue<int, 1, int>;
template class ring_queue<char, 1, signed char>;
struct point
{
  double x;
  double y;
};
template class ring_queue<point, 1024, std::size_t>;
}
}

namespace {
template<class T, std::size_t Capacity, class Index>
auto
pop_all(ring_queue<T, Capacity, Index>&) -> std::vector<T>;

TEST(test_ring_queue, new_queue_is_empty)
{
  auto queue = ring_queue<int, 64>{};
  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  EXPECT_THAT(contents, IsEmpty());
}

TEST(test_ring_queue, pop_returns_single_pushed_int)
{
  auto queue = ring_queue<int, 64>{};
  queue.push(1, [](auto data) noexcept->void { data[0] = 42; });

  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  ASSERT_THAT(contents, ElementsAre(42));
}

TEST(test_ring_queue, pop_returns_many_individually_pushed_ints)
{
  auto queue = ring_queue<int, 64>{};
  auto values_to_write = std::vector<int>{ 42, 9001, -1 };
  for (auto value : values_to_write) {
    queue.push(1, [value](auto data) noexcept->void { data[0] = value; });
  }

  auto written_values = pop_all(queue);
  EXPECT_THAT(written_values, ContainerEq(values_to_write));
}

TEST(test_ring_queue, pop_returns_all_bulk_pushed_ints)
{
  auto queue = ring_queue<int, 64>{};
  queue.push(3, [](auto data) noexcept->void {
    data[0] = 42;
    data[1] = 9001;
    data[2] = -1;
  });

  auto written_values = pop_all(queue);
  EXPECT_THAT(written_values, ElementsAre(42, 9001, -1));
}

TEST(test_ring_queue, bulk_pushing_at_end_of_ring_preserves_all_items)
{
  auto queue = ring_queue<int, 8>{};
  queue.push(6, [](auto) noexcept->void{});
  pop_all(queue);

  queue.push(4, [](auto data) noexcept->void {
    data[0] = 10;
    data[1] = 20;
    data[2] = 30;
    data[3] = 40;
  });

  auto written_values = pop_all(queue);
  EXPECT_THAT(written_values, ElementsAre(10, 20, 30, 40));
}

TEST(test_ring_queue, popping_again_returns_no_items)
{
  auto queue = ring_queue<int, 64>{};
  queue.push(5, [](auto data) noexcept->void {
    data[0] = 10;
    data[1] = 20;
    data[2] = 30;
    data[3] = 40;
    data[4] = 50;
  });

  auto contents = std::vector<int>{};
  queue.pop_all_into(contents);
  contents.clear();
  queue.pop_all_into(contents);
  EXPECT_THAT(contents, IsEmpty());
}

TEST(test_ring_queue, overflow_causes_pop_to_return_only_newest_data)
{
  auto queue = ring_queue<int, 4>{};
  for (auto value : { 10, 20, 30, 40, 50 }) {
    queue.push(1, [value](auto data) noexcept->void { data[0] = value; });
  }

  auto items = pop_all(queue);
  EXPECT_THAT(items, ElementsAre(20, 30, 40, 50));
}

template<std::size_t Capacity, class Index>
class test_ring_queue_against_reference : public testing::Test
{
protected:
  using size_type = Index;

  static constexpr auto capacity = Capacity;

  auto reset() -> void
  {
    this->queue = decltype(this->queue){};
    this->reference_queue = decltype(this->reference_queue){};
  }

  auto push_n(int count) -> void
  {
    for (auto i = 0; i < count; ++i) {
      this->queue.push(
        1, [this](auto data) noexcept->void { data[0] = this->cur_value; });
      this->reference_queue.push(this->cur_value);
      this->cur_value += 1;
    }
  }

  auto pop_all() -> void
  {
    ::pop_all(this->queue);
    reference_queue.clear();
  }

  auto pop_all_and_check(int max_size) -> void
  {
    auto popped = ::pop_all(this->queue);
    auto reference_popped = this->reference_queue.pop_n_and_clear(max_size);
    EXPECT_THAT(popped, ContainerEq(reference_popped));
  }

  ring_queue<int, capacity, size_type> queue{};
  reference_ring_queue<int, capacity> reference_queue{};
  int cur_value = 100;
};

using test_ring_queue_against_reference_basic =
  test_ring_queue_against_reference<8, int>;

TEST_F(test_ring_queue_against_reference_basic,
       overflow_causes_pop_to_return_only_newest_data_exhaustive)
{
  auto rng = cxxtrace::exhaustive_rng{};
  while (!rng.done() && !this->HasFailure()) {
    auto reader_offset = rng.next_integer_0(this->capacity * 5);
    SCOPED_TRACE(stringify("reader_offset = ", reader_offset));
    auto writer_lead = rng.next_integer_0(this->capacity * 5);
    SCOPED_TRACE(stringify("writer_lead = ", writer_lead));

    this->push_n(reader_offset);
    this->pop_all();

    this->push_n(writer_lead);
    this->pop_all_and_check(this->capacity);

    rng.lap();
    this->reset();
  }
}

TEST(test_ring_queue, overflowing_size_type_is_not_supported_yet)
{
  // TODO(strager): Instead of asserting, make overflowing the size type not
  // special at all.
  auto queue = ring_queue<int, 8, signed char>{};
  auto max_index = std::numeric_limits<decltype(queue)::size_type>::max();
  for (auto i = 0; i < max_index; ++i) {
    queue.push(1, [](auto data) noexcept { data[0] = 0; });
  }
  EXPECT_EXIT({ queue.push(1, [](auto data) noexcept { data[0] = 0; }); },
              testing::KilledBySignal(SIGABRT),
              "Writer overflowed size_type");
}

template<class T, std::size_t Capacity, class Index>
auto
pop_all(ring_queue<T, Capacity, Index>& queue) -> std::vector<T>
{
  auto data = std::vector<T>{};
  queue.pop_all_into(data);
  return data;
}
}
