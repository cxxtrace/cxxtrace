#include <cxxtrace/detail/ring_queue.h>
#include <string>

auto
main() -> int
{
  // CHECK: T must be a trivial type
  [[maybe_unused]] auto queue = cxxtrace::detail::ring_queue<std::string, 4>{};
  return 0;
}
