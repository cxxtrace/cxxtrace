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
  : private cxxtrace::detail::real_synchronization
  , public rseq_scheduler_synchronization_mixin<
      concurrency_stress_synchronization,
      cxxtrace::detail::real_synchronization::debug_source_location>
{
private:
  using real_synchronization = cxxtrace::detail::real_synchronization;
  using rseq_scheduler_synchronization_mixin = rseq_scheduler_synchronization_mixin<
      concurrency_stress_synchronization,
      real_synchronization::debug_source_location>;

public:
  using real_synchronization::atomic;
  using real_synchronization::atomic_flag;
  using real_synchronization::atomic_thread_fence;
  using real_synchronization::backoff;
  using real_synchronization::debug_source_location;
  using real_synchronization::nonatomic;
  using rseq_scheduler_synchronization_mixin::allow_preempt;
  using rseq_scheduler_synchronization_mixin::get_rseq;
  using rseq_scheduler_synchronization_mixin::rseq_handle;

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
};

extern template class rseq_scheduler_synchronization_mixin<
  concurrency_stress_synchronization,
  cxxtrace::detail::real_synchronization::debug_source_location>;
}

#endif
