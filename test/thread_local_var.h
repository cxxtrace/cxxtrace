#ifndef CXXTRACE_TEST_THREAD_LOCAL_VAR_H
#define CXXTRACE_TEST_THREAD_LOCAL_VAR_H

#include "cdschecker_thread_local_var.h"
#include "pthread_thread_local_var.h"
#include "relacy_thread_local_var.h"

namespace cxxtrace_test {
#if CXXTRACE_ENABLE_CDSCHECKER
template<class T>
using thread_local_var = cdschecker_thread_local_var<T>;
#elif CXXTRACE_ENABLE_CONCURRENCY_STRESS
template<class T>
using thread_local_var = pthread_thread_local_var<T>;
#elif CXXTRACE_ENABLE_RELACY
template<class T>
using thread_local_var = relacy_thread_local_var<T>;
#else
#error "Unknown platform"
#endif
}

#endif
