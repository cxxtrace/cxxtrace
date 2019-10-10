#ifndef CXXTRACE_DETAIL_ATOMIC_H
#define CXXTRACE_DETAIL_ATOMIC_H

#include <atomic>
#include <cxxtrace/detail/debug_source_location.h>

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

template<class T>
using atomic = synchronization::atomic<T>;
template<class T>
using nonatomic = synchronization::nonatomic<T>;

// Avoid ODR violation using an anonymous namespace. Referencing the
// synchronization type alias is effectively using #if.
namespace {
inline auto
atomic_thread_fence(std::memory_order memory_order,
                    debug_source_location caller) noexcept -> void
{
  synchronization::atomic_thread_fence(memory_order, caller);
}
}
}
}

#endif
