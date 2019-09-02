#ifndef CXXTRACE_DETAIL_CDSCHECKER_H
#define CXXTRACE_DETAIL_CDSCHECKER_H

#include <cstdint>

namespace cxxtrace {
namespace detail {
namespace cdschecker {
#if CXXTRACE_ENABLE_CDSCHECKER
enum memory_order
{
  memory_order_relaxed = 0,
  memory_order_acquire = 1,
  memory_order_release = 2,
  memory_order_acq_rel = 3,
  memory_order_seq_cst = 4,
};

using thrd_start_t = auto (*)(void*) -> void;
struct thrd_t
{
  void* data;
};

extern "C"
{
  auto model_fence_action(memory_order) -> void;
  auto model_init_action(void*, std::uint64_t) -> void;
  auto model_read_action(void*, memory_order) -> std::uint64_t;
  auto model_rmw_action(void*, memory_order, std::uint64_t) -> void;
  auto model_rmwc_action(void*, memory_order) -> void;
  auto model_rmwr_action(void*, memory_order) -> std::uint64_t;
  auto model_write_action(void*, memory_order, std::uint64_t) -> void;

  auto load_16(const void*) -> std::uint16_t;
  auto load_32(const void*) -> std::uint32_t;
  auto load_64(const void*) -> std::uint64_t;
  auto load_8(const void*) -> std::uint8_t;
  auto store_16(void*, std::uint16_t) -> void;
  auto store_32(void*, std::uint32_t) -> void;
  auto store_64(void*, std::uint64_t) -> void;
  auto store_8(void*, std::uint8_t) -> void;

  auto thrd_create(thrd_t*, thrd_start_t, void* opaque) -> int;
  auto thrd_current() -> thrd_t;
  auto thrd_join(thrd_t) -> int;
  auto thrd_yield() -> void;

  auto model_assert(bool, const char* file, int line) -> void;
}
#endif
}
}
}

#endif
