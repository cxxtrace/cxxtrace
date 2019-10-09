#ifndef CXXTRACE_DETAIL_ATOMIC_BASE_H
#define CXXTRACE_DETAIL_ATOMIC_BASE_H

#include <atomic>
#include <cxxtrace/detail/debug_source_location.h>
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

  auto compare_exchange_strong(T& expected,
                               T desired,
                               debug_source_location caller) noexcept -> bool
  {
    return static_cast<Class&>(*this).compare_exchange_strong(
      expected,
      desired,
      std::memory_order_seq_cst,
      std::memory_order_seq_cst,
      caller);
  }

  auto fetch_add(T addend, debug_source_location caller) noexcept -> T
  {
    return static_cast<Class&>(*this).fetch_add(
      addend, std::memory_order_seq_cst, caller);
  }

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
