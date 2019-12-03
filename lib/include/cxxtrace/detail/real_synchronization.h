#ifndef CXXTRACE_DETAIL_REAL_SYNCHRONIZATION_H
#define CXXTRACE_DETAIL_REAL_SYNCHRONIZATION_H

#include <atomic>
#include <cxxtrace/detail/atomic_base.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/atomic_ref.h>

// @@@ conditional
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/detail/rseq.h>

namespace cxxtrace {
namespace detail {
class real_synchronization
{
public:
  using debug_source_location = cxxtrace::detail::stub_debug_source_location;

  template<class T>
  class atomic;

  class atomic_flag;

  class backoff;

  template<class T>
  class nonatomic;

  // @@@ move out of line.
  class rseq_handle
  {
  public:
    explicit rseq_handle(::rseq* rseq)
      : rseq_{ rseq }
    {}

    [[gnu::always_inline]]
    auto get_current_processor_id(debug_source_location)
      -> cxxtrace::detail::processor_id
    {
      return registered_rseq::read_cpu_id(*this->rseq_);
    }

    [[gnu::always_inline]]
    auto get_rseq() -> ::rseq* { return this->rseq_; }

  private:
    ::rseq* rseq_;
  };

  static auto allow_preempt(debug_source_location) -> void;

  static auto atomic_thread_fence(std::memory_order,
                                  debug_source_location) noexcept -> void;

  // @@@ conditionally compile
  static auto get_rseq() -> rseq_handle
  {
    // @@@ hack!
    thread_local auto rr = registered_rseq::register_current_thread();
    return rseq_handle{ rr.get() };
  }
};

template<class T>
class real_synchronization::atomic : public atomic_base<T, atomic<T>>
{
private:
  using base = atomic_base<T, atomic<T>>;

public:
  using base::compare_exchange_strong;
  using base::fetch_add;
  using base::load;
  using base::store;

  [[gnu::always_inline]]
  explicit atomic() noexcept /* data uninitialized */ = default;

  [[gnu::always_inline]]
  /* implicit */ atomic(T value) noexcept
    : data{ value }
  {}

  [[gnu::always_inline]]
  auto compare_exchange_strong(T& expected,
                               T desired,
                               std::memory_order success_order,
                               std::memory_order failure_order,
                               debug_source_location) noexcept -> bool
  {
    return this->data.compare_exchange_strong(
      expected, desired, success_order, failure_order);
  }

  [[gnu::always_inline]]
  auto exchange(T desired,
                std::memory_order memory_order,
                debug_source_location) noexcept -> T
  {
    return this->data.exchange(desired, memory_order);
  }

  [[gnu::always_inline]]
  auto fetch_add(T addend,
                 std::memory_order memory_order,
                 debug_source_location) noexcept -> T
  {
    return this->data.fetch_add(addend, memory_order);
  }

  [[gnu::always_inline]]
  auto load(std::memory_order memory_order, debug_source_location) const
    noexcept -> T
  {
    // @@@ c'mon, brah
    return atomic_ref<T>{const_cast<std::atomic<T>&>(this->data)}.load(memory_order);
  }

  [[gnu::always_inline]]
  auto store(T value,
             std::memory_order memory_order,
             debug_source_location) noexcept -> void
  {
    atomic_ref<T>{this->data}.store(value, memory_order);
  }

private:
  std::atomic<T> data /* uninitialized */;
};

class real_synchronization::atomic_flag
{
public:
  [[gnu::always_inline]]
  explicit atomic_flag() noexcept = default;

  [[gnu::always_inline]]
  auto clear(std::memory_order order, debug_source_location) noexcept -> void
  {
    this->flag.clear(order);
  }

  [[gnu::always_inline]]
  auto test_and_set(std::memory_order order, debug_source_location) noexcept
    -> bool
  {
    return this->flag.test_and_set(order);
  }

private:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

class real_synchronization::backoff
{
public:
  explicit backoff();

  auto reset() -> void;
  auto yield(debug_source_location) -> void;
};

template<class T>
class real_synchronization::nonatomic : public nonatomic_base
{
public:
  [[gnu::always_inline]]
  explicit nonatomic() noexcept /* data uninitialized */ = default;

  [[gnu::always_inline]]
  /* implicit */ nonatomic(T value) noexcept
    : data{ value }
  {}

  [[gnu::always_inline]]
  auto load(debug_source_location) const noexcept -> T { return data; }

  [[gnu::always_inline]]
  auto store(T value, debug_source_location) noexcept -> void
  {
    this->data = value;
  }

private:
  T data /* uninitialized */;
};

[[gnu::always_inline]]
inline auto real_synchronization::allow_preempt(debug_source_location) -> void
{
  // Do nothing.
}

[[gnu::always_inline]]
inline auto
real_synchronization::atomic_thread_fence(std::memory_order memory_order,
                                          debug_source_location) noexcept
  -> void
{
  std::atomic_thread_fence(memory_order);
}
}
}

#endif
