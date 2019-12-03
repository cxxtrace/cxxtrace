#ifndef CXXTRACE_DETAIL_ATOMIC_REF_H
#define CXXTRACE_DETAIL_ATOMIC_REF_H

#include <atomic>

namespace cxxtrace {
namespace detail {
// TODO(strager): Switch to C++20's std::atomic_ref when available.
template<class T>
class atomic_ref
{
public:
  [[gnu::always_inline]]
  explicit atomic_ref(T& value) noexcept(false)
    : value_{ &value }
  {}

  // @@@ this constructor is non-standard (I think)
  [[gnu::always_inline]]
  explicit atomic_ref(std::atomic<T>& value) noexcept(false)
    : value_{ &reinterpret_cast<T &>(value) }
  {
    // @@@ assert size and alignment match (like old atomic_ref impl)
  }

  [[gnu::always_inline]]
  auto load(std::memory_order memory_order = std::memory_order_seq_cst) const
    noexcept -> T
  {
    // @@@ switch based on memory order
    (void)memory_order;
    return __atomic_load_n(this->value_, __ATOMIC_SEQ_CST);
  }

  [[gnu::always_inline]]
  auto store(T new_value,
             std::memory_order memory_order = std::memory_order_seq_cst) const
    noexcept -> void
  {
    // @@@ switch based on memory order
    (void)memory_order;
    __atomic_store(this->value_, &new_value, __ATOMIC_SEQ_CST);
  }

private:
  T* value_;
};

template<class T>
atomic_ref(const T&)->atomic_ref<T>;

template<class T>
atomic_ref(T&)->atomic_ref<T>;
}
}

#endif
