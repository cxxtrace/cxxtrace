#ifndef CXXTRACE_TEST_SYNCHRONIZATION_H
#define CXXTRACE_TEST_SYNCHRONIZATION_H

#if CXXTRACE_ENABLE_CDSCHECKER
#include "cdschecker_synchronization.h"
#endif

#if CXXTRACE_ENABLE_CONCURRENCY_STRESS
#include "concurrency_stress_synchronization.h"
#endif

#if CXXTRACE_ENABLE_RELACY
#include "relacy_synchronization.h"
#endif

namespace cxxtrace_test {
// To avoid ODR violations, avoid using concurrency_test_synchronization in
// header files.
using concurrency_test_synchronization =
#if CXXTRACE_ENABLE_CDSCHECKER
  cdschecker_synchronization
#elif CXXTRACE_ENABLE_CONCURRENCY_STRESS
  concurrency_stress_synchronization
#elif CXXTRACE_ENABLE_RELACY
  relacy_synchronization
#else
#error "Unknown platform"
#endif
  ;
}

#endif
