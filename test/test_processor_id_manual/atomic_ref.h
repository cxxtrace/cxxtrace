#ifndef CXXTRACE_TEST_ATOMIC_REF_H
#define CXXTRACE_TEST_ATOMIC_REF_H

#include <atomic>

namespace cxxtrace_test {
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

private:
  auto atomic() const noexcept -> std::atomic<T>&
  {
    return *reinterpret_cast<std::atomic<T>*>(this->value_);
  }

  T* value_;
};

template<class T>
atomic_ref(const T&)->atomic_ref<T>;

template<class T>
atomic_ref(T&)->atomic_ref<T>;
}

#endif
