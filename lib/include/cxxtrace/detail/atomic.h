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
template<class T>
using atomic = cdschecker_atomic<T>;
template<class T>
using nonatomic = cdschecker_nonatomic<T>;
// TODO(strager): Implement cdschecker_atomic_flag.
using atomic_flag = void;
#elif CXXTRACE_ENABLE_RELACY
template<class T>
using atomic = relacy_atomic<T>;
template<class T>
using nonatomic = relacy_nonatomic<T>;
// TODO(strager): Implement relacy_atomic_flag.
using atomic_flag = void;
#else
template<class T>
using atomic = real_atomic<T>;
template<class T>
using nonatomic = real_nonatomic<T>;
using atomic_flag = real_atomic_flag;
#endif

namespace { // Avoid ODR violation: #if inside an inline function's body.
inline auto
atomic_thread_fence(std::memory_order memory_order,
                    debug_source_location caller) noexcept -> void
{
#if CXXTRACE_ENABLE_RELACY
  relacy_atomic_thread_fence(memory_order, caller);
#elif CXXTRACE_ENABLE_CDSCHECKER
  cdschecker_atomic_thread_fence(memory_order, caller);
#else
  real_atomic_thread_fence(memory_order, caller);
#endif
}
}
}
}

#endif
