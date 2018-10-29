#include <cstdint>
#include <cxxtrace/detail/ring_queue.h>

auto
main() -> int
{
  // CHECK: Index must be able to contain non-negative integers less than
  // Capacity
  [[maybe_unused]] auto bad_queue =
    cxxtrace::detail::ring_queue<int, 257, unsigned char>{};
  // CHECK-NOT: Index must be able to contain non-negative integers less than
  // Capacity
  [[maybe_unused]] auto good_queue =
    cxxtrace::detail::ring_queue<int, 256, unsigned char>{};
  return 0;
}
