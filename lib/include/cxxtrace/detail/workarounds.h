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

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 8000
// Work around std::to_chars adding leading zeros.
// Bug report: https://bugs.llvm.org/show_bug.cgi?id=42166
#define CXXTRACE_WORK_AROUND_LIBCXX_42166 1
#endif

#endif
