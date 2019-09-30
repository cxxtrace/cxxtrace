#ifndef CXXTRACE_TEST_MEMORY_RESOURCE_FWD_H
#define CXXTRACE_TEST_MEMORY_RESOURCE_FWD_H

// HACK(strager): Including <experimental/memory_resource> causes problems due
// to CDSChecker's override of <atomic>. Avoid including that header where
// possible.
#if __has_include(<bits/c++config.h>)
// Import _GLIBCXX_BEGIN_NAMESPACE_VERSION from libstdc++.
#include <bits/c++config.h>
#endif
#if __has_include(<experimental/__config>)
// Import _LIBCPP_BEGIN_NAMESPACE_LFTS_PMR from libc++.
#include <experimental/__config>
#endif

#if CXXTRACE_ENABLE_CDSCHECKER && defined(_GLIBCXX_BEGIN_NAMESPACE_VERSION)
namespace std {
_GLIBCXX_BEGIN_NAMESPACE_VERSION
namespace experimental {
inline namespace fundamentals_v2 {
namespace pmr {
class memory_resource;
}
}
}
_GLIBCXX_END_NAMESPACE_VERSION
}
#elif CXXTRACE_ENABLE_CDSCHECKER && defined(_LIBCPP_BEGIN_NAMESPACE_LFTS_PMR)
_LIBCPP_BEGIN_NAMESPACE_LFTS_PMR
class memory_resource;
_LIBCPP_END_NAMESPACE_LFTS_PMR
#else
// @@@ iwyu pragmas to export
#include <experimental/memory_resource>
#endif

#endif
