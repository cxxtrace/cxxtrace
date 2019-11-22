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
  explicit atomic_ref(T& value) noexcept(false)
    : value_{ &value }
  {}

  auto load(std::memory_order memory_order = std::memory_order_seq_cst) const
    noexcept -> T
  {
    return this->atomic().load(memory_order);
  }

  auto store(T new_value,
             std::memory_order memory_order = std::memory_order_seq_cst) const
    noexcept -> void
  {
    this->atomic().store(new_value, memory_order);
  }

private:
  auto atomic() const noexcept -> std::atomic<T>&
  {
    using atomic_type = std::atomic<T>;
    static_assert(sizeof(atomic_type) == sizeof(T));
    static_assert(alignof(atomic_type) == alignof(T));
    return *reinterpret_cast<atomic_type*>(this->value_);
  }

  T* value_;
};

template<class T>
atomic_ref(const T&)->atomic_ref<T>;

template<class T>
atomic_ref(T&)->atomic_ref<T>;
}
}

#endif
