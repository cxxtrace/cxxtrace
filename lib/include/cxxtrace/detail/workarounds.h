#ifndef CXXTRACE_DETAIL_WORKAROUNDS_H
#define CXXTRACE_DETAIL_WORKAROUNDS_H

#if defined(__clang__) && __clang_major__ == 7 && __clang_minor__ == 0 &&      \
  defined(__APPLE__)
// Clang 7.0 with -O2 targeting macOS 10.11 generates many redundant calls to
// _tlv_get_addr. Attempt to avoid redundant calls to _tlv_get_addr, preferring
// to reuse registers instead.
#define CXXTRACE_WORK_AROUND_THREAD_LOCAL_OPTIMIZER 1
#endif

#if defined(__clang__) && __clang_major__ == 7 && __clang_minor__ == 0 &&      \
  defined(__APPLE__)
// Clang 7.0 targeting macOS 10.11 generates heavy-weight code to guard
// thread_local variables. Use the lazy_thread_local<> class instead which
// performs better than vanilla thread_local.
#define CXXTRACE_WORK_AROUND_SLOW_THREAD_LOCAL_GUARDS 1
#endif

#endif
