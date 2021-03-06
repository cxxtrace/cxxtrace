#ifndef CXXTRACE_TEST_CONCURRENCY_STRESS_SYNCHRONIZATION_H
#define CXXTRACE_TEST_CONCURRENCY_STRESS_SYNCHRONIZATION_H

#include "pthread_thread_local_var.h"
#include "rseq_scheduler_synchronization_mixin.h"
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/real_synchronization.h>

#if CXXTRACE_HAVE_LIBDISPATCH_SEMAPHORE
#include "libdispatch_semaphore.h"
#elif CXXTRACE_HAVE_POSIX_SEMAPHORE
#include "posix_semaphore.h"
#endif

namespace cxxtrace_test {
class concurrency_stress_synchronization
  : public cxxtrace::detail::real_synchronization
  , public rseq_scheduler_synchronization_mixin<
      concurrency_stress_synchronization,
      cxxtrace::detail::real_synchronization::debug_source_location>
{
public:
  using relaxed_semaphore =
#if CXXTRACE_HAVE_LIBDISPATCH_SEMAPHORE
    libdispatch_semaphore
#elif CXXTRACE_HAVE_POSIX_SEMAPHORE
    posix_semaphore
#else
#error "Unknown platform"
#endif
    ;

  template<class T>
  using thread_local_var = pthread_thread_local_var<T>;

  using rseq_scheduler_synchronization_mixin<
    concurrency_stress_synchronization,
    debug_source_location>::allow_preempt;
};

extern template class rseq_scheduler_synchronization_mixin<
  concurrency_stress_synchronization,
  cxxtrace::detail::real_synchronization::debug_source_location>;
}

#endif
