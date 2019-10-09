#ifndef CXXTRACE_DETAIL_CDSCHECKER_SYNCHRONIZATION_H
#define CXXTRACE_DETAIL_CDSCHECKER_SYNCHRONIZATION_H

#if CXXTRACE_ENABLE_CDSCHECKER
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cxxtrace/detail/atomic_base.h>
#include <cxxtrace/detail/cdschecker.h>
#include <cxxtrace/detail/debug_source_location.h>
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_ENABLE_CDSCHECKER
inline auto
cdschecker_cast(std::memory_order memory_order) noexcept
  -> cdschecker::memory_order
{
  switch (memory_order) {
    case std::memory_order_relaxed:
      return cdschecker::memory_order_relaxed;
    case std::memory_order_consume:
      // NOTE(strager): CDSChecker does not support memory_order_consume.
      return cdschecker::memory_order_acquire;
    case std::memory_order_acquire:
      return cdschecker::memory_order_acquire;
    case std::memory_order_release:
      return cdschecker::memory_order_release;
    case std::memory_order_acq_rel:
      return cdschecker::memory_order_acq_rel;
    case std::memory_order_seq_cst:
      return cdschecker::memory_order_seq_cst;
  }
  __builtin_unreachable();
}

template<class T>
class cdschecker_atomic : public atomic_base<T, cdschecker_atomic<T>>
{
private:
  using base = atomic_base<T, cdschecker_atomic<T>>;

public:
  using base::compare_exchange_strong;
  using base::fetch_add;
  using base::load;
  using base::store;

  explicit cdschecker_atomic() noexcept /* data uninitialized */ = default;

  /* implicit */ cdschecker_atomic(T value) noexcept
  {
    cdschecker::model_init_action(&this->data, this->from_t(value));
  }

  auto compare_exchange_strong(T& expected,
                               T desired,
                               std::memory_order success_order,
                               [[maybe_unused]] std::memory_order failure_order,
                               debug_source_location) noexcept -> bool
  {
    auto actual = this->to_t(cdschecker::model_rmwr_action(
      &this->data, cdschecker_cast(success_order)));
    if (actual == expected) {
      cdschecker::model_rmw_action(
        &this->data, cdschecker_cast(success_order), this->from_t(desired));
      return true;
    } else {
      // TODO(strager): Should we use failure_order here? Why does CDSChecker's
      // own implementation of atomic_compare_exchange_strong_explicit ignore
      // failure_order?
      cdschecker::model_rmwc_action(&this->data,
                                    cdschecker_cast(success_order));
      expected = actual;
      return false;
    }
  }

  auto fetch_add(T addend,
                 std::memory_order memory_order,
                 debug_source_location) noexcept -> T
  {
    auto original =
      cdschecker::model_rmwr_action(&this->data, cdschecker_cast(memory_order));
    auto modified = original + addend;
    cdschecker::model_rmw_action(
      &this->data, cdschecker_cast(memory_order), this->from_t(modified));
    return original;
  }

  auto load(std::memory_order memory_order, debug_source_location) const
    noexcept -> T
  {
    auto* data = const_cast<std::uint64_t*>(&this->data);
    return this->to_t(
      cdschecker::model_read_action(data, cdschecker_cast(memory_order)));
  }

  auto store(T value,
             std::memory_order memory_order,
             debug_source_location) noexcept -> void
  {
    cdschecker::model_write_action(
      &this->data, cdschecker_cast(memory_order), this->from_t(value));
  }

private:
  static auto to_t(std::uint64_t x) noexcept -> T
  {
    auto result = T{};
    std::memcpy(&result, &x, sizeof(result));
    return result;
    static_assert(sizeof(result) <= sizeof(x));
  }

  static auto from_t(T x) noexcept -> std::uint64_t
  {
    auto result = std::uint64_t{};
    std::memcpy(&result, &x, sizeof(x));
    return result;
    static_assert(sizeof(result) >= sizeof(x));
  }

  std::uint64_t data /* uninitialized */;
};

template<std::size_t Size>
struct cdschecker_nonatomic_traits;

template<>
struct cdschecker_nonatomic_traits<1>
{
  using value_type = std::uint8_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cdschecker::load_8(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cdschecker::store_8(&data, value);
  }
};

template<>
struct cdschecker_nonatomic_traits<2>
{
  using value_type = std::uint16_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cdschecker::load_16(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cdschecker::store_16(&data, value);
  }
};

template<>
struct cdschecker_nonatomic_traits<4>
{
  using value_type = std::uint32_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cdschecker::load_32(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cdschecker::store_32(&data, value);
  }
};

template<>
struct cdschecker_nonatomic_traits<8>
{
  using value_type = std::uint64_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cdschecker::load_64(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cdschecker::store_64(&data, value);
  }
};

template<class T>
class cdschecker_nonatomic : public nonatomic_base
{
private:
  using traits = cdschecker_nonatomic_traits<sizeof(T)>;
  using storage_type = typename traits::value_type;

public:
  explicit cdschecker_nonatomic() /* data uninitialized */ = default;

  /* implicit */ cdschecker_nonatomic(T value) noexcept
    : storage{ this->value_to_storage(value) }
  {}

  auto load(debug_source_location) const noexcept -> T
  {
    return this->storage_to_value(traits::load(this->storage));
  }

  auto store(T value, debug_source_location) noexcept -> void
  {
    traits::store(this->storage, this->value_to_storage(value));
  }

private:
  static auto storage_to_value(storage_type storage) noexcept -> T
  {
    T value;
    static_assert(sizeof(value) == sizeof(storage));
    std::memcpy(&value, &storage, sizeof(value));
    return value;
  }

  static auto value_to_storage(T value) noexcept -> storage_type
  {
    storage_type storage;
    static_assert(sizeof(storage) == sizeof(value));
    std::memcpy(&storage, &value, sizeof(storage));
    return storage;
  }

  storage_type storage /* uninitialized */;
};

inline auto
cdschecker_atomic_thread_fence(std::memory_order memory_order,
                               debug_source_location) noexcept -> void
{
  cdschecker::model_fence_action(cdschecker_cast(memory_order));
}
#endif
}
}

#endif
