#ifndef CXXTRACE_DETAIL_RING_QUEUE_H
#define CXXTRACE_DETAIL_RING_QUEUE_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cxxtrace/detail/add.h>
#include <cxxtrace/detail/queue_sink.h>
#include <limits>
#include <type_traits>
#include <vector>

namespace cxxtrace {
namespace detail {
// A special-purpose, lossy, bounded FIFO container optimized for writes.
//
// Special-purpose: Items in a ring_queue must be trivial. Constructors and
// destructors are not called on a ring_queue's items.
//
// Lossy: If a writer pushes too many items, older items are discarded.
//
// Bounded: The maximum number of items allowed in a ring_queue is fixed.
// Operations on a ring_queue will never allocate memory.
//
// FIFO: Items are read First In, First Out.
//
// ring_queue is not thread-safe.
//
// TODO(strager): Add an API for the reader to detect when items are discarded.
//
// @see spmc_ring_queue
template<class T, std::size_t Capacity, class Index = int>
class ring_queue
{
public:
  static_assert(Capacity > 0);
  static_assert(std::is_integral_v<Index>, "Index must be an integral type");
  static_assert(
    std::numeric_limits<Index>::max() >= Capacity - 1,
    "Index must be able to contain non-negative integers less than Capacity");
  static_assert(std::is_trivial_v<T>, "T must be a trivial type");

  using size_type = Index;
  using value_type = T;

  static inline constexpr const auto capacity = size_type{ Capacity };

  template<class WriterFunction>
  auto push(size_type count, WriterFunction&& write) noexcept -> void
  {
    auto vindex = this->begin_push(count);
    write(push_handle{ this->storage, vindex });
  }

  auto reset() noexcept -> void
  {
    this->read_vindex = 0;
    this->write_vindex = 0;
  }

  auto pop_all_into(std::vector<T>& output) -> void
  {
    this->pop_all_into(vector_queue_sink<T>{ output });
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    assert(this->read_vindex <= this->write_vindex);

    auto begin_vindex = size_type{};
    if (this->write_vindex > capacity) {
      begin_vindex =
        std::max(static_cast<size_type>(this->write_vindex - capacity),
                 this->read_vindex);
    } else {
      begin_vindex = this->read_vindex;
    }
    auto end_vindex = this->write_vindex;

    output.reserve(end_vindex - begin_vindex);
    for (auto i = begin_vindex; i < end_vindex; ++i) {
      output.push_back(this->storage[i % this->capacity]);
    }
    this->read_vindex = end_vindex;
  }

private:
  class push_handle
  {
  public:
    auto set(size_type index, T value) noexcept -> void
    {
      this->storage[(this->write_vindex + index) % capacity] = std::move(value);
    }

  private:
    explicit push_handle(std::array<value_type, capacity>& storage,
                         size_type write_vindex) noexcept
      : storage{ storage }
      , write_vindex{ write_vindex }
    {}

    std::array<value_type, capacity>& storage;
    size_type write_vindex{ 0 };

    friend class ring_queue;
  };

  auto begin_push(size_type count) noexcept -> size_type
  {
    assert(count > 0);
    assert(count < this->capacity);

    auto old_write_vindex = this->write_vindex;

    auto maybe_new_write_vindex = add(this->write_vindex, count);
    if (!maybe_new_write_vindex.has_value()) {
      this->abort_due_to_overflow();
    }
    this->write_vindex = *maybe_new_write_vindex;

    return old_write_vindex;
  }

  [[noreturn]] auto abort_due_to_overflow() const noexcept -> void
  {
    std::fprintf(stderr, "fatal: Writer overflowed size_type\n");
    std::abort();
  }

  // 'vindex' is an abbreviation for 'virtual index'.
  size_type read_vindex{ 0 };
  size_type write_vindex{ 0 };

  std::array<value_type, capacity> storage
    /* uninitialized */;
};
}
}

#endif
