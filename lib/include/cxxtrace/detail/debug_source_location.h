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
class stub_debug_source_location
{};

#if CXXTRACE_ENABLE_RELACY
#define CXXTRACE_HERE (RL_INFO)
#else
#define CXXTRACE_HERE (::cxxtrace::detail::stub_debug_source_location{})
#endif
}
}

#endif
