#ifndef CXXTRACE_DETAIL_LAZY_THREAD_LOCAL_H
#define CXXTRACE_DETAIL_LAZY_THREAD_LOCAL_H

#include <cstdint>
#include <cxxtrace/detail/workarounds.h>
#include <type_traits>

#if CXXTRACE_WORK_AROUND_SLOW_THREAD_LOCAL_GUARDS
namespace cxxtrace {
namespace detail {
template<class T>
auto
destroy_object_on_thread_exit(T*) noexcept -> void;

template<class T, class Tag>
class lazy_thread_local
{
public:
  static_assert(std::is_default_constructible_v<T>);

  static auto get() -> T*
  {
    auto* storage = get_thread_data_storage();
    if (!storage->is_initialized) {
      initialize(storage);
    }
    return storage->data_pointer_unsafe();
  }

private:
  struct data_storage
  {
    auto data_pointer_unsafe() noexcept -> T*
    {
      return reinterpret_cast<T*>(&this->storage);
    }

    bool is_initialized /* uninitialized */;
    std::aligned_storage_t<sizeof(T), alignof(T)> storage /* uninitialized */;
  };

  static_assert(std::is_trivial_v<data_storage>,
                "data_storage must be trivial in order for lazy_thread_local "
                "to have low overhead");

  __attribute__((noinline)) static auto initialize(data_storage* storage)
    -> void
  {
    auto* data = storage->data_pointer_unsafe();
    new (data) T{};
    storage->is_initialized = true;
    destroy_object_on_thread_exit(data);
  }

  static auto on_thread_exit(void* object) noexcept -> void
  {
    reinterpret_cast<T*>(object)->~T();
  }

  static auto get_thread_data_storage() noexcept -> data_storage*
  {
    thread_local data_storage storage;
    auto* storage_pointer = &storage;
#if CXXTRACE_WORK_AROUND_THREAD_LOCAL_OPTIMIZER
    asm volatile("" : "+r"(storage_pointer));
#endif
    return storage_pointer;
  }
};

#if defined(__APPLE__)
extern "C"
{
  __attribute__((visibility("hidden"))) extern std::int8_t __dso_handle;

  auto _tlv_atexit(void (*callback)(void*) noexcept,
                   void* opaque,
                   void* dso_handle) noexcept -> int;
}

template<class T>
auto
destroy_object_on_thread_exit(T* object) noexcept -> void
{
  auto on_thread_exit = [](void* opaque) noexcept->void
  {
    auto* object = reinterpret_cast<T*>(opaque);
    object->~T();
  };
  _tlv_atexit(on_thread_exit, static_cast<void*>(object), &__dso_handle);
}
#else
#error "Unknown platform"
#endif
}
}
#endif

#endif
