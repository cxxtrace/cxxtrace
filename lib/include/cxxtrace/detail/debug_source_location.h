#ifndef CXXTRACE_DETAIL_DEBUG_SOURCE_LOCATION_H
#define CXXTRACE_DETAIL_DEBUG_SOURCE_LOCATION_H

#if CXXTRACE_ENABLE_RELACY
#include <cxxtrace/detail/warning.h>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wdeprecated-declarations")
CXXTRACE_WARNING_IGNORE_CLANG("-Wdollar-in-identifier-extension")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wsized-deallocation")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")

#include <relacy/base.hpp>

CXXTRACE_WARNING_POP
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_ENABLE_RELACY
using debug_source_location = rl::debug_info;
#define CXXTRACE_HERE (RL_INFO)
#else
class stub_debug_source_location
{};
using debug_source_location = stub_debug_source_location;
#define CXXTRACE_HERE (::cxxtrace::detail::debug_source_location{})
#endif
}
}

#endif
