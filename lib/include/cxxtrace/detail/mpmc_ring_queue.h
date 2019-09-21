#ifndef CXXTRACE_DETAIL_MPMC_RING_QUEUE_H
#define CXXTRACE_DETAIL_MPMC_RING_QUEUE_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cxxtrace/detail/add.h>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/molecular.h>
#include <cxxtrace/detail/mutex.h>
#include <limits>
#include <optional>
#include <type_traits> // IWYU pragma: keep
#include <utility>
// IWYU pragma: no_forward_declare cxxtrace::detail::molecular

namespace cxxtrace {
namespace detail {
enum class mpmc_ring_queue_push_result : bool
{
  not_pushed_due_to_contention = false,
  pushed = true,
};

// A special-purpose, lossy, bounded, MPMC FIFO container optimized for
// uncontended writes.
//
// Special-purpose: Items in a mpmc_ring_queue must be trivial. Constructors and
// destructors are not called on a mpmc_ring_queue's items.
//
// Lossy: If a writer pushes too many items, older items are discarded.
//
// Bounded: The maximum number of items allowed in a mpmc_ring_queue is fixed.
// Operations on a mpmc_ring_queue will never allocate memory.
//
// MPMC: Zero or more threads can push items ("Multiple Producer"), and zero or
// more threads can concurrently pop items ("Multiple Consumer").
//
// FIFO: Items are read First In, First Out.
//
// TODO(strager): Add an API for the reader to detect when items are discarded.
//
// @see ring_queue
// @see spsc_ring_queue
template<class T, std::size_t Capacity, class Index = int>
class mpmc_ring_queue
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
  using push_result = mpmc_ring_queue_push_result;

  static inline constexpr const auto capacity = size_type{ Capacity };

  auto reset() noexcept -> void
  {
    this->read_vindex.store(0, CXXTRACE_HERE);
    this->write_begin_vindex.store(0, CXXTRACE_HERE);
    this->write_end_vindex.store(0, CXXTRACE_HERE);
  }

  template<class WriterFunction>
  auto try_push(size_type count, WriterFunction&& write) noexcept -> push_result
  {
    auto vindexes = this->begin_push(count);
    if (!vindexes.has_value()) {
      return push_result::not_pushed_due_to_contention;
    }
    auto [begin_vindex, end_vindex] = *vindexes;
    write(push_handle{ this->storage, begin_vindex });
    this->end_push(end_vindex);
    return push_result::pushed;
  }

  template<class Sink>
  auto pop_all_into(Sink&& output) -> void
  {
    // TODO(strager): Consolidate duplication with spsc_ring_queue.
    // TODO(strager): Relax memory ordering as appropriate.
    auto guard = lock_guard<mutex>{ this->consumer_mutex, CXXTRACE_HERE };

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
        this->write_begin_vindex.load(CXXTRACE_HERE);
      const auto write_end_vindex = this->write_end_vindex.load(CXXTRACE_HERE);
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

    // FIXME(strager): This fence (and the fence in begin_push) should be
    // redundant. Why does
    // ring_queue_overflow_drops_some_but_not_all_items_relacy_test fail with
    // CDSChecker without this fence? Doesn't the implicitly-seq_cst
    // compare_exchange_strong enforce acq_rel ordering already?
    atomic_thread_fence(std::memory_order_acq_rel, CXXTRACE_HERE);

    {
      const auto write_end_vindex = this->write_end_vindex.load(CXXTRACE_HERE);
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
  class push_handle
  {
  public:
    auto set(size_type index, T value) noexcept -> void
    {
      this->storage[(this->write_begin_vindex + index) % capacity].store(
        std::move(value), CXXTRACE_HERE);
    }

  private:
    explicit push_handle(std::array<molecular<value_type>, capacity>& storage,
                         size_type write_begin_vindex) noexcept
      : storage{ storage }
      , write_begin_vindex{ write_begin_vindex }
    {}

    std::array<molecular<value_type>, capacity>& storage;
    size_type write_begin_vindex{ 0 };

    friend class mpmc_ring_queue;
  };

  auto begin_push(size_type count) noexcept
    -> std::optional<std::pair<size_type, size_type>>
  {
    assert(count > 0);
    assert(count < this->capacity);

    auto write_begin_vindex =
      this->write_begin_vindex.load(std::memory_order_seq_cst, CXXTRACE_HERE);
    auto maybe_new_write_end_vindex = add(write_begin_vindex, count);
    if (!maybe_new_write_end_vindex.has_value()) {
      this->abort_due_to_overflow();
    }
    if (!this->write_end_vindex.compare_exchange_strong(
          write_begin_vindex, *maybe_new_write_end_vindex, CXXTRACE_HERE)) {
      return std::nullopt;
    }

    // FIXME(strager): This fence (and the fence in pop_all_into) should be
    // redundant. Why does
    // ring_queue_overflow_drops_some_but_not_all_items_relacy_test fail with
    // CDSChecker without this fence? Doesn't the implicitly-seq_cst
    // compare_exchange_strong enforce acq_rel ordering already?
    atomic_thread_fence(std::memory_order_acq_rel, CXXTRACE_HERE);

    return std::pair{ write_begin_vindex, *maybe_new_write_end_vindex };
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

  mutex consumer_mutex;

  std::array<molecular<value_type>, capacity> storage
    /* uninitialized */;
};
}
}

#endif
