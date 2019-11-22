#ifndef CXXTRACE_TEST_GTEST_SCOPED_TRACE_H
#define CXXTRACE_TEST_GTEST_SCOPED_TRACE_H

#include "cxxtrace_cpp.h"
#include <cxxtrace/string.h>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

// CXXTRACE_SCOPED_TRACE behaves like gtest's SCOPED_TRACE, except it allows
// iostream-style formatting.
//
// Usage:
//
// CXXTRACE_SCOPED_TRACE() << "data: " << data;
#define CXXTRACE_SCOPED_TRACE()                                                \
  auto CXXTRACE_CPP_PASTE(_cxxtrace_scoped_trace_, __LINE__) =                 \
    ::cxxtrace_test::detail::scoped_trace_builder{ __FILE__, __LINE__ } ^=     \
    std::ostringstream()

namespace cxxtrace_test {
namespace detail {
struct scoped_trace_builder
{
  cxxtrace::czstring file;
  int line;

  auto operator^=(std::ostream& message_stream) const -> testing::ScopedTrace
  {
    return this->make_scoped_trace(std::move(message_stream));
  }

  auto operator^=(std::ostream&& message_stream) const -> testing::ScopedTrace
  {
    return this->make_scoped_trace(std::move(message_stream));
  }

  auto make_scoped_trace(std::ostream&& message_stream) const
    -> testing::ScopedTrace
  {
    auto message = dynamic_cast<std::ostringstream&&>(message_stream).str();
    return testing::ScopedTrace(this->file, this->line, std::move(message));
  }
};
}
}

#endif
