#ifndef CXXTRACE_TEST_RELACY_BACKOFF_H
#define CXXTRACE_TEST_RELACY_BACKOFF_H

#if CXXTRACE_ENABLE_RELACY
#include "cxxtrace_concurrency_test_base.h" // IWYU pragma: export
#include "pretty_type_name.h"
#include <cassert>
#include <cxxtrace/detail/atomic.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/warning.h>
#include <cxxtrace/detail/workarounds.h>
#include <exception>
#include <experimental/memory_resource>
#include <iosfwd>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdynamic-exception-spec")
CXXTRACE_WARNING_IGNORE_CLANG("-Wextra-semi")
CXXTRACE_WARNING_IGNORE_CLANG("-Winline-new-delete")
CXXTRACE_WARNING_IGNORE_CLANG("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wmissing-field-initializers")
CXXTRACE_WARNING_IGNORE_GCC("-Wsized-deallocation")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")

// clang-format off
#include <relacy/thread_local_ctx.hpp>
// clang-format on
#include <relacy/backoff.hpp>
#include <relacy/context.hpp>
#include <relacy/context_base_impl.hpp>
#include <relacy/stdlib/condition_variable.hpp>
#include <relacy/stdlib/event.hpp>
#include <relacy/stdlib/mutex.hpp>

CXXTRACE_WARNING_POP
#endif

namespace cxxtrace_test {
#if CXXTRACE_ENABLE_RELACY
class relacy_backoff
{
public:
  explicit relacy_backoff();
  ~relacy_backoff();

  auto reset() -> void;
  auto yield(cxxtrace::detail::debug_source_location) -> void;

private:
  rl::linear_backoff backoff;
};
#endif
}

#endif
