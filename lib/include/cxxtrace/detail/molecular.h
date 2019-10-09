#ifndef CXXTRACE_DETAIL_MOLECULAR_H
#define CXXTRACE_DETAIL_MOLECULAR_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace cxxtrace {
namespace detail {
template<std::size_t Size>
class largest_lock_free_atomic_value_type_impl;

template<std::size_t Size>
using largest_lock_free_atomic_value_type =
  typename largest_lock_free_atomic_value_type_impl<Size>::type;

// molecular<T> owns a T object and provides data-race-free access to copies.
//
// molecular<T> is similar to atomic<T>, but loads and stores may be striped.
// Use outside synchronization to detect striping.
template<class T, class Sync>
class molecular
{
private:
  template<class U>
  using atomic = typename Sync::template atomic<U>;
  using debug_source_location = typename Sync::debug_source_location;

public:
  using value_type = T;

  static_assert(std::is_trivial_v<value_type>);

  // Implies std::memory_order_relaxed.
  auto load(debug_source_location caller) const noexcept -> value_type
  {
    value_type value;
    auto* value_elements = reinterpret_cast<storage_element_type*>(&value);
    for (auto i = std::size_t{ 0 }; i < this->storage_size; ++i) {
      auto temp = this->storage[i].load(std::memory_order_relaxed, caller);
      auto is_last_element = i == this->storage_size - 1;
      if (is_last_element && this->storage_contains_trailing_padding) {
        std::memcpy(&value_elements[i],
                    &temp,
                    sizeof(storage_element_type) -
                      this->storage_trailing_padding_size);
      } else {
        std::memcpy(&value_elements[i], &temp, sizeof(temp));
      }
    }
    return value;
  }

  // Implies std::memory_order_relaxed.
  auto store(value_type value, debug_source_location caller) noexcept -> void
  {
    auto* value_elements =
      reinterpret_cast<const storage_element_type*>(&value);
    for (auto i = std::size_t{ 0 }; i < this->storage_size; ++i) {
      storage_element_type temp;
      auto is_last_element = i == this->storage_size - 1;
      if (is_last_element && this->storage_contains_trailing_padding) {
        std::memcpy(&temp,
                    &value_elements[i],
                    sizeof(storage_element_type) -
                      this->storage_trailing_padding_size);
      } else {
        std::memcpy(&temp, &value_elements[i], sizeof(temp));
      }
      this->storage[i].store(temp, std::memory_order_relaxed, caller);
    }
  }

private:
  using storage_element_type =
    detail::largest_lock_free_atomic_value_type<sizeof(T)>;
  inline static constexpr auto storage_size =
    (sizeof(T) + sizeof(storage_element_type) - 1) /
    sizeof(storage_element_type);

  using nonatomic_storage_type = std::array<storage_element_type, storage_size>;
  static_assert(sizeof(nonatomic_storage_type) >= sizeof(value_type),
                "nonatomic_storage_type should be able to contain value_type");
  static_assert(sizeof(nonatomic_storage_type) <
                  sizeof(value_type) + sizeof(storage_element_type),
                "nonatomic_storage_type should reasonable padding, if any");
  inline static constexpr auto storage_trailing_padding_size =
    sizeof(nonatomic_storage_type) - sizeof(value_type);
  inline static constexpr auto storage_contains_trailing_padding =
    storage_trailing_padding_size > 0;

  using atomic_storage_type =
    std::array<atomic<storage_element_type>, storage_size>;

  atomic_storage_type storage /* uninitialized */;
};

template<std::size_t Size>
class largest_lock_free_atomic_value_type_impl
{
public:
  using type = std::conditional_t<
    Size >= sizeof(unsigned long),
    unsigned long,
    std::conditional_t<Size >= sizeof(unsigned),
                       unsigned,
                       std::conditional_t<Size >= sizeof(unsigned short),
                                          unsigned short,
                                          unsigned char>>>;

  static_assert(ATOMIC_CHAR_LOCK_FREE >= 1, "Unsupported platform");
  static_assert(ATOMIC_SHORT_LOCK_FREE >= 1, "Unsupported platform");
  static_assert(ATOMIC_INT_LOCK_FREE >= 1, "Unsupported platform");
  static_assert(ATOMIC_LONG_LOCK_FREE >= 1, "Unsupported platform");
};
}
}

#endif
