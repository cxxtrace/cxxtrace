#ifndef CXXTRACE_DETAIL_ATOMIC_BASE_H
#define CXXTRACE_DETAIL_ATOMIC_BASE_H

#include <atomic>
#include <type_traits> // IWYU pragma: keep

namespace cxxtrace {
namespace detail {
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

  template<class DebugSourceLocation>
  auto compare_exchange_strong(T& expected,
                               T desired,
                               DebugSourceLocation caller) noexcept -> bool
  {
    return static_cast<Class&>(*this).compare_exchange_strong(
      expected,
      desired,
      std::memory_order_seq_cst,
      std::memory_order_seq_cst,
      caller);
  }

  template<class DebugSourceLocation>
  auto fetch_add(T addend, DebugSourceLocation caller) noexcept -> T
  {
    return static_cast<Class&>(*this).fetch_add(
      addend, std::memory_order_seq_cst, caller);
  }

  template<class DebugSourceLocation>
  auto load(DebugSourceLocation caller) const noexcept -> T
  {
    return static_cast<const Class&>(*this).load(std::memory_order_seq_cst,
                                                 caller);
  }

  template<class DebugSourceLocation>
  auto store(T value, DebugSourceLocation caller) noexcept -> void
  {
    static_cast<Class&>(*this).store(value, std::memory_order_seq_cst, caller);
  }
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
}
}

#endif
