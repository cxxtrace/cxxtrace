#ifndef CXXTRACE_DETAIL_WORKAROUNDS_H
#define CXXTRACE_DETAIL_WORKAROUNDS_H

#if __has_include(<version>)
#include <version>
#else
#include <ciso646>
#endif

#if defined(__clang__) && (__clang_major__ == 7 || __clang_major__ == 8) &&    \
  defined(__APPLE__)
// Clang 7.0 and Clang 8.0.0 with -O2 targeting macOS 10.11 or macOS 10.12
// generate many redundant calls to _tlv_get_addr. Attempt to avoid redundant
// calls to _tlv_get_addr, preferring to reuse registers instead.
#define CXXTRACE_WORK_AROUND_THREAD_LOCAL_OPTIMIZER 1
#endif

#if defined(__clang__) && (__clang_major__ == 7 || __clang_major__ == 8) &&    \
  defined(__APPLE__)
// Clang 7.0 and Clang 8.0.0 targeting macOS 10.11 or macOS 10.12 generate
// heavy-weight code to guard thread_local variables. Use the
// lazy_thread_local<> class instead which performs better than vanilla
// thread_local.
#define CXXTRACE_WORK_AROUND_SLOW_THREAD_LOCAL_GUARDS 1
#endif

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 8000
// libc++ 8.0.0 does not declare std::atomic<T>::value_type.
#define CXXTRACE_WORK_AROUND_ATOMIC_VALUE_TYPE 1
#endif

#if CXXTRACE_ENABLE_CDSCHECKER
// CDSChecker's definition of thrd_join in <threads.h> does not match the C11
// standard.
#define CXXTRACE_WORK_AROUND_CDCHECKER_THRD_JOIN 1

// CDSChecker's definition of thread_start_t in <threads.h> does not match the
// C11 standard.
#define CXXTRACE_WORK_AROUND_CDCHECKER_THRD_START_T 1

// CDSChecker's <threads.h> lacks a definition of thrd_success.
#define CXXTRACE_WORK_AROUND_MISSING_THRD_ENUM 1

// CDSChecker's definition of std::atomic<T>::load in <atomic> is not marked
// const, but C++11's definition is marked const.
#define CXXTRACE_WORK_AROUND_NON_CONST_STD_ATOMIC_LOAD 1
#endif

#endif
