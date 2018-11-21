#ifndef CXXTRACE_DETAIL_ATOMIC_H
#define CXXTRACE_DETAIL_ATOMIC_H

#include <atomic>
#include <utility>

#if CXXTRACE_ENABLE_RELACY
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wdynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winline-new-delete"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <relacy/atomic.hpp>
#include <relacy/atomic_fence.hpp>
#include <relacy/var.hpp>

#pragma clang diagnostic pop
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_ENABLE_RELACY
using debug_source_location = rl::debug_info;
#define CXXTRACE_HERE (RL_INFO)
#else
class stub_debug_source_location
{};
using debug_source_location = stub_debug_source_location;
#define CXXTRACE_HERE (::cxxtrace::detail::debug_source_location{})
#endif

template<class T, class Class>
class atomic_base
{
public:
  static_assert(std::is_trivial_v<T>);

  explicit atomic_base() noexcept = default;

  atomic_base(const atomic_base&) = delete;
  atomic_base& operator=(const atomic_base&) = delete;
  atomic_base(atomic_base&&) = delete;
  atomic_base& operator=(atomic_base&&) = delete;

  auto load(debug_source_location caller) const noexcept -> T
  {
    return static_cast<const Class&>(*this).load(std::memory_order_seq_cst,
                                                 caller);
  }

  auto store(T value, debug_source_location caller) noexcept -> void
  {
    static_cast<Class&>(*this).store(value, std::memory_order_seq_cst, caller);
  }
};

template<class T>
class real_atomic : public atomic_base<T, real_atomic<T>>
{
private:
  using base = atomic_base<T, real_atomic<T>>;

public:
  using base::load;
  using base::store;

  explicit real_atomic() noexcept /* data uninitialized */ = default;

  explicit real_atomic(T value) noexcept
    : data{ value }
  {}

  auto load(std::memory_order memory_order, debug_source_location) const
    noexcept -> T
  {
    return this->data.load(memory_order);
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

class nonatomic_base
{
public:
  explicit nonatomic_base() noexcept = default;

  nonatomic_base(const nonatomic_base&) = delete;
  nonatomic_base& operator=(const nonatomic_base&) = delete;
  nonatomic_base(nonatomic_base&&) = delete;
  nonatomic_base& operator=(nonatomic_base&&) = delete;
};

template<class T>
class real_nonatomic : public nonatomic_base
{
public:
  explicit real_nonatomic() noexcept /* data uninitialized */ = default;

  explicit real_nonatomic(T value) noexcept
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

#if CXXTRACE_ENABLE_RELACY
inline auto
relacy_cast(std::memory_order memory_order) noexcept -> rl::memory_order
{
  switch (memory_order) {
    case std::memory_order_relaxed:
      return rl::mo_relaxed;
    case std::memory_order_consume:
      return rl::mo_consume;
    case std::memory_order_acquire:
      return rl::mo_acquire;
    case std::memory_order_release:
      return rl::mo_release;
    case std::memory_order_acq_rel:
      return rl::mo_acq_rel;
    case std::memory_order_seq_cst:
      return rl::mo_seq_cst;
  }
  __builtin_unreachable();
}

template<class T>
class relacy_atomic : public atomic_base<T, relacy_atomic<T>>
{
private:
  using base = atomic_base<T, relacy_atomic<T>>;

public:
  using base::load;
  using base::store;

  explicit relacy_atomic() /* data uninitialized */ = default;

  explicit relacy_atomic(T value) noexcept
    : data{ value }
  {}

  auto load(std::memory_order memory_order, debug_source_location caller) const
    noexcept -> T
  {
    return this->data.load(relacy_cast(memory_order), caller);
  }

  auto store(T value,
             std::memory_order memory_order,
             debug_source_location caller) noexcept -> void
  {
    this->data.store(value, relacy_cast(memory_order), caller);
  }

private:
  rl::atomic<T> data /* uninitialized */;
};

template<class T>
class relacy_nonatomic : public nonatomic_base
{
public:
  explicit relacy_nonatomic() /* data uninitialized */ = default;

  explicit relacy_nonatomic(T value) noexcept
    : data{ value }
  {}

  auto load(debug_source_location caller) const noexcept -> T
  {
    return this->data(caller).load();
  }

  auto store(T value, debug_source_location caller) noexcept -> void
  {
    this->data(caller).store(value);
  }

private:
  rl::var<T> data /* uninitialized */;
};

inline auto
relacy_atomic_thread_fence(std::memory_order memory_order,
                           debug_source_location caller) noexcept -> void
{
  rl::atomic_thread_fence(relacy_cast(memory_order), caller);
}
#endif

#if CXXTRACE_ENABLE_RELACY
template<class T>
using atomic = relacy_atomic<T>;
template<class T>
using nonatomic = relacy_nonatomic<T>;
#else
template<class T>
using atomic = real_atomic<T>;
template<class T>
using nonatomic = real_nonatomic<T>;
#endif

namespace { // Avoid ODR violation: #if inside an inline function's body.
inline auto
atomic_thread_fence(std::memory_order memory_order,
                    debug_source_location caller) noexcept -> void
{
#if CXXTRACE_ENABLE_RELACY
  relacy_atomic_thread_fence(memory_order, caller);
#else
  real_atomic_thread_fence(memory_order, caller);
#endif
}
}
}
}

#endif
