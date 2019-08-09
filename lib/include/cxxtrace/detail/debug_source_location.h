#ifndef CXXTRACE_DETAIL_DEBUG_SOURCE_LOCATION_H
#define CXXTRACE_DETAIL_DEBUG_SOURCE_LOCATION_H

#if CXXTRACE_ENABLE_RELACY
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <relacy/base.hpp>

#pragma clang diagnostic pop
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
