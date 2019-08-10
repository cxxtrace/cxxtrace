#ifndef CXXTRACE_DETAIL_APPLE_COMMPAGE_H
#define CXXTRACE_DETAIL_APPLE_COMMPAGE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cxxtrace {
namespace detail {
#if defined(__APPLE__) && defined(__x86_64__)
// See darwin-xnu/osfmk/i386/cpu_capabilities.h:
// https://github.com/apple/darwin-xnu/blob/a449c6a3b8014d9406c2ddbdc81795da24aa7443/osfmk/i386/cpu_capabilities.h#L176
struct apple_commpage
{
  static constexpr auto base = std::uintptr_t{ 0x00007fffffe00000ULL };

  static inline const auto* signature =
    reinterpret_cast<const std::byte*>(base + 0x000);
  static constexpr auto signature_size = std::size_t{ 0x10 };

  static inline const auto* version =
    reinterpret_cast<const std::uint16_t*>(base + 0x01e);

  static inline const volatile auto* sched_gen =
    reinterpret_cast<const std::atomic<std::uint32_t>*>(base + 0x028);

  static_assert(std::decay_t<decltype(*sched_gen)>::is_always_lock_free);
  static_assert(sizeof(*sched_gen) == 4);
};
#endif
}
}

#endif
