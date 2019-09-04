#ifndef CXXTRACE_DETAIL_HAVE_H
#define CXXTRACE_DETAIL_HAVE_H

#if defined(__APPLE__)
// _tlv_atexit
#define CXXTRACE_HAVE_TLV_ATEXIT 1
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

#if defined(__APPLE__)
// ::pthread_getname_np(...)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_GETNAME_NP 1
#endif

#if defined(__APPLE__)
// ::pthread_setname_np(...)
// <pthread.h>
#define CXXTRACE_HAVE_PTHREAD_SETNAME_NP 1
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

#if defined(__APPLE__)
// ::close(...)
// <unistd.h>
#define CXXTRACE_HAVE_POSIX_FD 1
#endif

#endif
