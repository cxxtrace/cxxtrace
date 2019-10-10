#ifndef CXXTRACE_DETAIL_ATOMIC_H
#define CXXTRACE_DETAIL_ATOMIC_H

#if CXXTRACE_ENABLE_CDSCHECKER
#include <cxxtrace/detail/cdschecker_synchronization.h> // IWYU pragma: export
#endif

#if CXXTRACE_ENABLE_RELACY
#include <cxxtrace/detail/relacy_synchronization.h> // IWYU pragma: export
#endif

#if !(CXXTRACE_ENABLE_CDSCHECKER || CXXTRACE_ENABLE_RELACY)
#include <cxxtrace/detail/real_synchronization.h> // IWYU pragma: export
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_ENABLE_CDSCHECKER
using synchronization = cdschecker_synchronization;
#elif CXXTRACE_ENABLE_RELACY
using synchronization = relacy_synchronization;
#else
using synchronization = real_synchronization;
#endif
}
}

#endif
