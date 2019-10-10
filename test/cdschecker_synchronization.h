#ifndef CXXTRACE_TEST_CDSCHECKER_SYNCHRONIZATION_H
#define CXXTRACE_TEST_CDSCHECKER_SYNCHRONIZATION_H

#if CXXTRACE_ENABLE_CDSCHECKER
#include "cdschecker_thread.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/atomic_base.h>
#include <cxxtrace/detail/cdschecker.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>
#include <cxxtrace/detail/workarounds.h>
#include <exception>
#include <pthread.h>
#include <system_error>
#include <type_traits>
#endif

namespace cxxtrace_test {
#if CXXTRACE_ENABLE_CDSCHECKER
class cdschecker_synchronization
{
public:
  using debug_source_location = cxxtrace::detail::debug_source_location;

  template<class T>
  class atomic;

  template<class T>
  class nonatomic;

  template<class T>
  class thread_local_var;

  static auto atomic_thread_fence(std::memory_order,
                                  debug_source_location) noexcept -> void;

private:
  template<std::size_t Size>
  struct nonatomic_traits;

  static auto cdschecker_cast(std::memory_order memory_order) noexcept
    -> cxxtrace::detail::cdschecker::memory_order;
};

inline auto
cdschecker_synchronization::cdschecker_cast(
  std::memory_order memory_order) noexcept
  -> cxxtrace::detail::cdschecker::memory_order
{
  switch (memory_order) {
    case std::memory_order_relaxed:
      return cxxtrace::detail::cdschecker::memory_order_relaxed;
    case std::memory_order_consume:
      // NOTE(strager): CDSChecker does not support memory_order_consume.
      return cxxtrace::detail::cdschecker::memory_order_acquire;
    case std::memory_order_acquire:
      return cxxtrace::detail::cdschecker::memory_order_acquire;
    case std::memory_order_release:
      return cxxtrace::detail::cdschecker::memory_order_release;
    case std::memory_order_acq_rel:
      return cxxtrace::detail::cdschecker::memory_order_acq_rel;
    case std::memory_order_seq_cst:
      return cxxtrace::detail::cdschecker::memory_order_seq_cst;
  }
  __builtin_unreachable();
}

template<class T>
class cdschecker_synchronization::atomic
  : public cxxtrace::detail::atomic_base<T, atomic<T>>
{
private:
  using base = cxxtrace::detail::atomic_base<T, atomic<T>>;

public:
  using base::compare_exchange_strong;
  using base::fetch_add;
  using base::load;
  using base::store;

  explicit atomic() noexcept /* data uninitialized */ = default;

  /* implicit */ atomic(T value) noexcept
  {
    cxxtrace::detail::cdschecker::model_init_action(&this->data,
                                                    this->from_t(value));
  }

  auto compare_exchange_strong(T& expected,
                               T desired,
                               std::memory_order success_order,
                               [[maybe_unused]] std::memory_order failure_order,
                               debug_source_location) noexcept -> bool
  {
    auto actual = this->to_t(cxxtrace::detail::cdschecker::model_rmwr_action(
      &this->data, cdschecker_cast(success_order)));
    if (actual == expected) {
      cxxtrace::detail::cdschecker::model_rmw_action(
        &this->data, cdschecker_cast(success_order), this->from_t(desired));
      return true;
    } else {
      // TODO(strager): Should we use failure_order here? Why does CDSChecker's
      // own implementation of atomic_compare_exchange_strong_explicit ignore
      // failure_order?
      cxxtrace::detail::cdschecker::model_rmwc_action(
        &this->data, cdschecker_cast(success_order));
      expected = actual;
      return false;
    }
  }

  auto fetch_add(T addend,
                 std::memory_order memory_order,
                 debug_source_location) noexcept -> T
  {
    auto original = cxxtrace::detail::cdschecker::model_rmwr_action(
      &this->data, cdschecker_cast(memory_order));
    auto modified = original + addend;
    cxxtrace::detail::cdschecker::model_rmw_action(
      &this->data, cdschecker_cast(memory_order), this->from_t(modified));
    return original;
  }

  auto load(std::memory_order memory_order, debug_source_location) const
    noexcept -> T
  {
    auto* data = const_cast<std::uint64_t*>(&this->data);
    return this->to_t(cxxtrace::detail::cdschecker::model_read_action(
      data, cdschecker_cast(memory_order)));
  }

  auto store(T value,
             std::memory_order memory_order,
             debug_source_location) noexcept -> void
  {
    cxxtrace::detail::cdschecker::model_write_action(
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

template<>
struct cdschecker_synchronization::nonatomic_traits<1>
{
  using value_type = std::uint8_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cxxtrace::detail::cdschecker::load_8(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cxxtrace::detail::cdschecker::store_8(&data, value);
  }
};

template<>
struct cdschecker_synchronization::nonatomic_traits<2>
{
  using value_type = std::uint16_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cxxtrace::detail::cdschecker::load_16(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cxxtrace::detail::cdschecker::store_16(&data, value);
  }
};

template<>
struct cdschecker_synchronization::nonatomic_traits<4>
{
  using value_type = std::uint32_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cxxtrace::detail::cdschecker::load_32(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cxxtrace::detail::cdschecker::store_32(&data, value);
  }
};

template<>
struct cdschecker_synchronization::nonatomic_traits<8>
{
  using value_type = std::uint64_t;

  static auto load(const value_type& data) noexcept -> value_type
  {
    return cxxtrace::detail::cdschecker::load_64(&data);
  }

  static auto store(value_type& data, value_type value) noexcept -> void
  {
    return cxxtrace::detail::cdschecker::store_64(&data, value);
  }
};

template<class T>
class cdschecker_synchronization::nonatomic
  : public cxxtrace::detail::nonatomic_base
{
private:
  using traits = nonatomic_traits<sizeof(T)>;
  using storage_type = typename traits::value_type;

public:
  explicit nonatomic() /* data uninitialized */ = default;

  /* implicit */ nonatomic(T value) noexcept
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

template<class T>
class cdschecker_synchronization::thread_local_var
{
public:
  static_assert(std::is_trivial_v<T>);

  auto operator-> () noexcept -> T* { return this->get(); }

  auto operator*() noexcept -> T& { return *this->get(); }

  auto get() noexcept -> T*
  {
    auto current_thread_id = cdschecker_current_thread_id();
    assert(current_thread_id >= this->first_thread_id);
    auto slot_index = current_thread_id - this->first_thread_id;
    // If this assertion fails, increase maximum_thread_count.
    assert(std::size_t(slot_index) < this->slots_.size());
    auto& slot = this->slots_[slot_index];

    if (!slot.allocated) {
      slot.allocated = true;
      std::memset(&slot.value, 0, sizeof(slot.value));
    }
    return &slot.value;
  }

private:
  struct slot
  {
    T value /* uninitialized */;
    bool allocated{ false };
  };

  // Thread ID 0 is unused.
  // Thread ID 1 is used by the main thread which spawns each test thread. See
  // run_concurrency_test_from_cdschecker.
  static constexpr auto first_thread_id = cdschecker_thread_id{ 2 };

  static constexpr auto maximum_thread_count = 3;

  std::array<slot, maximum_thread_count> slots_;
};

inline auto
cdschecker_synchronization::atomic_thread_fence(std::memory_order memory_order,
                                                debug_source_location) noexcept
  -> void
{
  cxxtrace::detail::cdschecker::model_fence_action(
    cdschecker_cast(memory_order));
}
#endif
}

#endif
