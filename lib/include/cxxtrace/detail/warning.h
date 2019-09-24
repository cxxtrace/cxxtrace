#ifndef CXXTRACE_DETAIL_WARNING_H
#define CXXTRACE_DETAIL_WARNING_H

#if defined(__clang__)
#define CXXTRACE_WARNING_PUSH _Pragma("clang diagnostic push")
#define CXXTRACE_WARNING_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define CXXTRACE_WARNING_PUSH _Pragma("GCC diagnostic push")
#define CXXTRACE_WARNING_POP _Pragma("GCC diagnostic pop")
#else
#error "Unknown platform"
#endif

#if defined(__clang__)
#define CXXTRACE_WARNING_IGNORE_CLANG(warning_name)                            \
  _Pragma(                                                                     \
    CXXTRACE_WARNING_PRAGMA_STRING(clang diagnostic ignored, warning_name))
#else
#define CXXTRACE_WARNING_IGNORE_CLANG(warning_name)
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define CXXTRACE_WARNING_IGNORE_GCC(warning_name)                              \
  _Pragma(CXXTRACE_WARNING_PRAGMA_STRING(GCC diagnostic ignored, warning_name))
#else
#define CXXTRACE_WARNING_IGNORE_GCC(warning_name)
#endif

#define CXXTRACE_WARNING_PRAGMA_STRING(x, y)                                   \
  CXXTRACE_WARNING_PRAGMA_STRING_(x y)
#define CXXTRACE_WARNING_PRAGMA_STRING_(x) #x

#endif
