#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_allocator.h"
#include <array>
#include <cstddef>
#include <cxxtrace/detail/warning.h>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
#include "mymemory.h"
CXXTRACE_WARNING_POP

namespace cxxtrace_test {
// CDSChecker's realloc() fails before CDSChecker's main() is called. (malloc()
// and calloc() work fine.) Work around this bug by creating a separate heap for
// all CDSCheck-intercepted allocations.
//
// This workaround only works reliably if CDSChecker's main() is never called.
auto
configure_default_allocator() -> void
{
  static auto heap = std::array<std::byte, 64 * 1024>{};
  ::user_snapshot_space =
    ::create_mspace_with_base(heap.data(), heap.size(), true);
}
}
