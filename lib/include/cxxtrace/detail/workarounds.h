#ifndef CXXTRACE_DETAIL_WORKAROUNDS_H
#define CXXTRACE_DETAIL_WORKAROUNDS_H

#if __has_include(<version>)
#include <version> // IWYU pragma: export
#else
#include <ciso646> // IWYU pragma: export
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

// At the time of writing, C++'s std::void_t<a> is indistinguishable from
// std::void_t<b> when comparing class template partial specializations. See CWG
// 1980: http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#1980
#define CXXTRACE_WORK_AROUND_VOID_T_EQUIVALENCE 1

#if defined(__x86_64__) &&                                                     \
  (defined(__clang__) || defined(__GCC_ASM_FLAG_OUTPUTS__))
// In some cases, Clang 8.0 generates inefficienct code for
// std::atomic_flag::test_and_set:
//
// > mov $0x1, %dl
// > xchg %dl, (lock)
// > test $0x1, %dl
// > jne lock_not_acquired
//
// In other cases, Clang 8.0 and GCC 9.2 generate slightly better code for
// xchg-based std::atomic_flag::test_and_set:
//
// > mov $1, %eax
// > xchg %al, (lock)
// > test %al, %al
// > jne lock_not_acquired
//
// Work around this sometimes-inefficient code generation.
//
// Additionally, on my mobile Ivy Bridge CPU (Intel(R) Core(TM) i7-3720QM CPU @
// 2.60GHz), bts is faster than xchg for implementing
// std::atomic_flag::test_and_set. Even better, bts requires fewer
// instructions:
//
// > lock bts $0, (lock)
// > jc lock_not_acquired
//
// Improve the performance of spin locks by using bts.
//
// TODO(strager): Benchmark bts on other processors.
#define CXXTRACE_WORK_AROUND_SLOW_ATOMIC_FLAG 1
#endif

#endif
