#ifndef CXXTRACE_DETAIL_HAVE_H
#define CXXTRACE_DETAIL_HAVE_H

#if defined(__APPLE__)
// _tlv_atexit
#define CXXTRACE_HAVE_TLV_ATEXIT 1
#endif

#if defined(__linux__) && defined(_GNU_SOURCE)
// <cxxabi.h>
// ::__cxxabiv1::__cxa_thread_atexit
#define CXXTRACE_HAVE_CXA_THREAD_ATEXIT 1
#endif

#if defined(__APPLE__)
#define CXXTRACE_HAVE_APPLE_COMMPAGE 1
#endif

#if defined(__APPLE__)
// ::sysctlbyname(...)
// <sys/sysctl.h>
#define CXXTRACE_HAVE_SYSCTL 1
#endif

#if defined(__APPLE__)
// ::mach_thread_self(...)
// ::thread_info(...)
// <mach/thread_act.h>
// THREAD_EXTENDED_INFO
// THREAD_IDENTIFIER_INFO
#define CXXTRACE_HAVE_MACH_THREAD 1
#endif

#if defined(__APPLE__)
// ::pthread_threadid_np(...)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_THREADID_NP 1
#endif

#if defined(__APPLE__) || (defined(__linux__) && defined(_GNU_SOURCE))
// ::pthread_getname_np(...)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_GETNAME_NP 1
#endif

#if defined(__APPLE__) || (defined(__linux__) && defined(_GNU_SOURCE))
// ::pthread_setname_np(...)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_SETNAME_NP 1
#endif

#if defined(__APPLE__)
// ::pthread_setname_np(const char*)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_SETNAME_NP_1 1
#endif

#if defined(__linux__) && defined(_GNU_SOURCE)
// ::pthread_setname_np(::pthread_t, const char*)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_SETNAME_NP_2 1
#endif

#if defined(__APPLE__)
// ::proc_pidinfo(...)
// <libproc.h>
// <sys/proc_info.h>
// PROC_PIDTHREADID64INFO
#define CXXTRACE_HAVE_PROC_PIDINFO 1
#endif

#if defined(__APPLE__)
// ::getpid(...)
// <unistd.h>
#define CXXTRACE_HAVE_GETPID 1
#endif

#if defined(__APPLE__)
// ::mach_absolute_time(...)
// <mach/mach_time.h>
#define CXXTRACE_HAVE_MACH_TIME 1
#endif

#if defined(__APPLE__)
// ::OSSpinLock
// <libkern/OSSpinLockDeprecated.h>
#define CXXTRACE_HAVE_OS_SPIN_LOCK 1
#endif

#if defined(__APPLE__)
// ::os_unfair_lock
// <os/lock.h>
#define CXXTRACE_HAVE_OS_UNFAIR_LOCK 1
#endif

#if defined(__APPLE__)
// ::_NSGetExecutablePath(...)
// <mach-o/dyld.h>
#define CXXTRACE_HAVE_NS_GET_EXECUTABLE_PATH 1
#endif

#if defined(__APPLE__) || defined(__linux__)
// ::close(...)
// <unistd.h>
#define CXXTRACE_HAVE_POSIX_FD 1
#endif

#if defined(__linux__)
#define CXXTRACE_HAVE_LINUX_PROCFS 1
#endif

#if defined(__linux__) && defined(_GNU_SOURCE)
// <sys/syscall.h>
// SYS_gettid
#define CXXTRACE_HAVE_SYSCALL_GETTID 1
#endif

#if defined(__linux__) && defined(_GNU_SOURCE)
// ::get_nprocs_conf(...)
// <sys/sysinfo.h>
#define CXXTRACE_HAVE_GET_NPROCS_CONF 1
#endif

#endif
