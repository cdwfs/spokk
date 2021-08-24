#include "spokk_platform.h"

// Platform-specific header files
#if   defined(ZOMBO_OS_WINDOWS)
#   include <processthreadsapi.h>
#   include <profileapi.h>
#   include <synchapi.h>
#   include <sysinfoapi.h>
#elif defined(ZOMBO_OS_POSIX) || defined(ZOMBO_OS_APPLE)
#   include <sys/types.h>

#   include <sys/stat.h> // for _stat()
#   include <ctype.h>
#   include <pthread.h>
#   include <time.h>
#   include <unistd.h>
#else
#   error Unsupported platform
#endif

// zomboCpuCount()
int32_t zomboCpuCount(void)
{
#if   defined(ZOMBO_OS_WINDOWS)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
#   error Unsupported compiler
#endif
}

// zomboClockTicks()
uint64_t zomboClockTicks(void)
{
#if   defined(ZOMBO_OS_WINDOWS)
    uint64_t outTicks;
    QueryPerformanceCounter((LARGE_INTEGER*)&outTicks);
    return outTicks;
#elif defined(ZOMBO_OS_APPLE)
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    return (uint64_t)mts.tv_nsec + (uint64_t)mts.tv_sec*1000000000ULL;
#elif defined(ZOMBO_OS_POSIX)
#   if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
    struct timespec ts;
    clock_gettime(1, &ts);
    return (uint64_t)ts.tv_nsec + (uint64_t)ts.tv_sec*1000000000ULL;
#   else
#       error no timer here!
#   endif
#else
#   error Unsupported compiler
#endif
}

// zomboTicksToSeconds()
double zomboTicksToSeconds(uint64_t ticks)
{
#if   defined(ZOMBO_OS_WINDOWS)
    LARGE_INTEGER qpcFreq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&qpcFreq);
    return (double)ticks / (double)qpcFreq.QuadPart;
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    return (double)ticks / 1e9;
#else
#   error Unsupported compiler
#endif
}

// zomboProcessId()
int zomboProcessId(void)
{
#if   defined(ZOMBO_OS_WINDOWS)
    return GetCurrentProcessId();
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    return getpid();
#else
#   error Unsupported compiler
#endif
}

// zomboThreadId()
int zomboThreadId(void)
{
#if   defined(ZOMBO_OS_WINDOWS)
    return GetCurrentThreadId();
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    return (int)(intptr_t)pthread_self();
#else
#   error Unsupported compiler
#endif
}

// zomboSleepMsec()
void zomboSleepMsec(uint32_t msec)
{
#if   defined(ZOMBO_OS_WINDOWS)
    Sleep(msec);
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    struct timespec ts = {0, msec*1000};
    nanosleep(&ts, NULL);
#else
#   error Unsupported compiler
#endif
}

// zomboFopen()
FILE *zomboFopen(const char *path, const char *mode)
{
#if   defined(ZOMBO_OS_WINDOWS)
    FILE *f = NULL;
    errno_t ferr = fopen_s(&f, path, mode);
    return (ferr == 0) ? f : NULL;
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    return fopen(path, mode);
#endif
}

// zomboGetEnv()
char* zomboGetEnv(const char* varname)
{
#if   defined(ZOMBO_OS_WINDOWS)
    return getenv(varname);
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
    return getenv(varname);
#endif
}
