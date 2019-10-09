#ifndef CXXTRACE_DETAIL_REAL_SYNCHRONIZATION_H
#define CXXTRACE_DETAIL_REAL_SYNCHRONIZATION_H

#include <atomic>
#include <cxxtrace/detail/atomic_base.h>
#include <cxxtrace/detail/debug_source_location.h>

namespace cxxtrace {
namespace detail {
template<class T>
class real_atomic : public atomic_base<T, real_atomic<T>>
{
private:
  using base = atomic_base<T, real_atomic<T>>;

public:
  using base::compare_exchange_strong;
  using base::fetch_add;
  using base::load;
  using base::store;

  explicit real_atomic() noexcept /* data uninitialized */ = default;

  /* implicit */ real_atomic(T value) noexcept
    : data{ value }
  {}

  auto compare_exchange_strong(T& expected,
                               T desired,
                               std::memory_order success_order,
                               std::memory_order failure_order,
                               debug_source_location) noexcept -> bool
  {
    return this->data.compare_exchange_strong(
      expected, desired, success_order, failure_order);
  }

  auto exchange(T desired,
                std::memory_order memory_order,
                debug_source_location) noexcept -> T
  {
    return this->data.exchange(desired, memory_order);
  }

  auto fetch_add(T addend,
                 std::memory_order memory_order,
                 debug_source_location) noexcept -> T
  {
    return this->data.fetch_add(addend, memory_order);
  }

  auto load(std::memory_order memory_order, debug_source_location) const
    noexcept -> T
  {
    auto& data = this->data;
    return data.load(memory_order);
  }

  auto store(T value,
             std::memory_order memory_order,
             debug_source_location) noexcept -> void
  {
    this->data.store(value, memory_order);
  }

private:
  std::atomic<T> data /* uninitialized */;
};

class real_atomic_flag
{
public:
  explicit real_atomic_flag() noexcept = default;

  auto clear(std::memory_order order, debug_source_location) noexcept -> void
  {
    this->flag.clear(order);
  }

  auto test_and_set(std::memory_order order, debug_source_location) noexcept
    -> bool
  {
    return this->flag.test_and_set(order);
  }

private:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

template<class T>
class real_nonatomic : public nonatomic_base
{
public:
  explicit real_nonatomic() noexcept /* data uninitialized */ = default;

  /* implicit */ real_nonatomic(T value) noexcept
    : data{ value }
  {}

  auto load(debug_source_location) const noexcept -> T { return data; }

  auto store(T value, debug_source_location) noexcept -> void
  {
    this->data = value;
  }

private:
  T data /* uninitialized */;
};

inline auto
real_atomic_thread_fence(std::memory_order memory_order,
                         debug_source_location) noexcept -> void
{
  std::atomic_thread_fence(memory_order);
}
}
}

#endif