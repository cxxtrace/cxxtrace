#ifndef CXXTRACE_TEST_CDSCHECKER_BACKOFF_H
#define CXXTRACE_TEST_CDSCHECKER_BACKOFF_H

#if CXXTRACE_ENABLE_CDSCHECKER
#include "cxxtrace_concurrency_test_base.h" // IWYU pragma: export
#include "pretty_type_name.h"
#include <cassert>
#include <cxxtrace/detail/cdschecker.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <cxxtrace/detail/workarounds.h>
#include <cxxtrace/string.h>
#include <exception>
#include <experimental/memory_resource>
#include <iosfwd>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>
#endif

namespace cxxtrace_test {
#if CXXTRACE_ENABLE_CDSCHECKER
class cdschecker_backoff
{
public:
  explicit cdschecker_backoff();
  ~cdschecker_backoff();

  auto reset() -> void;
  auto yield(cxxtrace::detail::debug_source_location) -> void;
};
#endif
}

#endif
