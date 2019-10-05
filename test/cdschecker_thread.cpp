#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_thread.h"
#include <cxxtrace/detail/warning.h>
#include <modeltypes.h>
#include <type_traits>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")
#include "threads-model.h"

CXXTRACE_WARNING_POP

namespace cxxtrace_test {
static_assert(std::is_same_v<cdschecker_thread_id, ::thread_id_t>);

auto
cdschecker_current_thread_id() noexcept -> cdschecker_thread_id
{
  return ::thread_current()->get_id();
}
}
