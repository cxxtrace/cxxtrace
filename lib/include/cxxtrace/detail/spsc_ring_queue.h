#ifndef CXXTRACE_DETAIL_SPSC_RING_QUEUE_H
#define CXXTRACE_DETAIL_SPSC_RING_QUEUE_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cxxtrace/detail/add.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/molecular.h>
#include <cxxtrace/detail/real_synchronization.h>
#include <limits>
#include <type_traits> // IWYU pragma: keep
#include <utility>
// IWYU pragma: no_forward_declare cxxtrace::detail::molecular

namespace cxxtrace {
namespace detail {
// A special-purpose, lossy, bounded, SPSC FIFO container optimized for writes.
//
// Special-purpose: Items in a spsc_ring_queue must be trivial. Constructors and
// destructors are not called on a spsc_ring_queue's items.
//
// Lossy: If a writer pushes too many items, older items are discarded.
//
// Bounded: The maximum number of items allowed in a spsc_ring_queue is fixed.
// Operations on a spsc_ring_queue will never allocate memory.
//
// SPSC: A single thread can push items ("Single Producer"), and a single thread
// can pop items ("Single Consumer").
//
// FIFO: Items are read First In, First Out.
//
// TODO(strager): Add an API for the reader to detect when items are discarded.
//
// @see ring_queue
// @see mpsc_ring_queue
template<class T,
         std::size_t Capacity,
         class Index = int,
         class Sync = real_synchronization>
class spsc_ring_queue
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

  auto reset() noexcept -> void
  {
    this->read_vindex.store(0, CXXTRACE_HERE);
    this->write_begin_vindex.store(0, CXXTRACE_HERE);
    this->write_end_vindex.store(0, CXXTRACE_HERE);
  }

  template<class WriterFunction>
  auto push(size_type count, WriterFunction&& write) noexcept -> void
  {
    auto [begin_vindex, end_vindex] = this->begin_push(count);
    write(push_handle{ this->storage, begin_vindex });
    this->end_push(end_vindex);
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    // TODO(strager): Consolidate duplication with mpsc_ring_queue.
    auto read_vindex = this->read_vindex.load(CXXTRACE_HERE);

    auto get_begin_vindex = [&read_vindex](
                              size_type write_end_vindex) noexcept->size_type
    {
      if (write_end_vindex > capacity) {
        return std::max(static_cast<size_type>(write_end_vindex - capacity),
                        read_vindex);
      } else {
        return read_vindex;
      }
    };

    auto begin_vindex = size_type{};
    auto end_vindex = size_type{};
    {
      const auto write_begin_vindex =
        this->write_begin_vindex.load(std::memory_order_acquire, CXXTRACE_HERE);
      const auto write_end_vindex =
        this->write_end_vindex.load(std::memory_order_acquire, CXXTRACE_HERE);
      assert(read_vindex <= write_end_vindex);
      assert(write_begin_vindex <= write_end_vindex);

      begin_vindex = get_begin_vindex(write_end_vindex);
      end_vindex = write_begin_vindex;
      output.reserve(end_vindex - begin_vindex);
      for (auto i = begin_vindex; i < end_vindex; ++i) {
        output.push_back(this->storage[i % this->capacity].load(CXXTRACE_HERE));
      }
    }
    auto output_item_count = end_vindex - begin_vindex;

    Sync::atomic_thread_fence(std::memory_order_seq_cst, CXXTRACE_HERE);

    {
      const auto write_end_vindex =
        this->write_end_vindex.load(std::memory_order_relaxed, CXXTRACE_HERE);
      if (write_end_vindex != end_vindex) {
        // push was called concurrently. Undo potentially-corrupted reads in the
        // output.
        auto new_begin_vindex = get_begin_vindex(write_end_vindex);
        assert(new_begin_vindex >= begin_vindex);
        auto items_to_unoutput =
          std::min(new_begin_vindex - begin_vindex, output_item_count);
        output.pop_front_n(items_to_unoutput);
      }
    }

    this->read_vindex.store(end_vindex, CXXTRACE_HERE);
  }

private:
  template<class U>
  using atomic = typename Sync::template atomic<U>;
  template<class U>
  using nonatomic = typename Sync::template nonatomic<U>;

  class push_handle
  {
  public:
    auto set(size_type index, T value) noexcept -> void
    {
      this->storage[(this->write_begin_vindex + index) % capacity].store(
        std::move(value), CXXTRACE_HERE);
    }

  private:
    explicit push_handle(
      std::array<molecular<value_type, Sync>, capacity>& storage,
      size_type write_begin_vindex) noexcept
      : storage{ storage }
      , write_begin_vindex{ write_begin_vindex }
    {}

    std::array<molecular<value_type, Sync>, capacity>& storage;
    size_type write_begin_vindex{ 0 };

    friend class spsc_ring_queue;
  };

  auto begin_push(size_type count) noexcept -> std::pair<size_type, size_type>
  {
    assert(count > 0);
    assert(count < this->capacity);

    auto write_begin_vindex =
      this->write_begin_vindex.load(std::memory_order_relaxed, CXXTRACE_HERE);
    auto maybe_new_write_end_vindex = add(write_begin_vindex, count);
    if (!maybe_new_write_end_vindex.has_value()) {
      this->abort_due_to_overflow();
    }
    this->write_end_vindex.store(
      *maybe_new_write_end_vindex, std::memory_order_relaxed, CXXTRACE_HERE);
    Sync::atomic_thread_fence(std::memory_order_acq_rel, CXXTRACE_HERE);

    return { write_begin_vindex, *maybe_new_write_end_vindex };
  }

  auto end_push(size_type write_end_vindex) noexcept -> void
  {
    this->write_begin_vindex.store(
      write_end_vindex, std::memory_order_release, CXXTRACE_HERE);
  }

  [[noreturn]] auto abort_due_to_overflow() const noexcept -> void
  {
    std::fprintf(stderr, "fatal: Writer overflowed size_type\n");
    std::abort();
  }

  // 'vindex' is an abbreviation for 'virtual index'.
  nonatomic<size_type> read_vindex{ 0 };
  atomic<size_type> write_begin_vindex{ 0 };
  atomic<size_type> write_end_vindex{ 0 };

  std::array<molecular<value_type, Sync>, capacity> storage
    /* uninitialized */;
};
}
}

#endif
