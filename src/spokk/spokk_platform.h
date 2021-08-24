#if !defined(SPOKK_PLATFORM_H)
#define SPOKK_PLATFORM_H
/* Collection of cross-platform functions and macros */

// clang-format off
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if   defined(ZOMBO_STATIC)
#   define ZOMBO_DEF static
#else
#   define ZOMBO_DEF extern
#endif

#if   defined(_MSC_VER)
#   if !defined(_AMD64_)
#       define _AMD64_
#   endif
#   include <windef.h>
#   include <debugapi.h>
#   include <direct.h>
#   include <sys/types.h>
#   include <sys/stat.h> // for _stat()
#   define ZOMBO_OS_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
//#   include <mach/clock.h>
//#   include <mach/mach.h>
#   define ZOMBO_OS_APPLE
#elif defined(unix) || defined(__unix__) || defined(__unix)
#   include <unistd.h>
#   if   defined(_POSIX_VERSION)
#       define ZOMBO_OS_POSIX
#   else
#       error Unsupported OS (non-POSIX Unix)
#   endif
#   include <sys/types.h>
#   include <sys/stat.h>  // for _stat()
#elif defined(__ANDROID__)
#   define ZOMBO_OS_ANDROID
#else
#   error Unsupported OS
#endif

#if   defined(__amd64__) || defined(_AMD64_)
#   define ZOMBO_ARCH_X64
#elif defined(__aarch64__)
#   define ZOMBO_ARCH_ARM64
#elif defined(__arm__)
#   define ZOMBO_ARCH_ARM
#else
#   error Unsupported CPU architecture
#endif

#if   defined(_MSC_VER)
#   define ZOMBO_COMPILER_MSVC
#elif defined(__clang__)
#   define ZOMBO_COMPILER_CLANG
#elif defined(__GNUC__)
#   define ZOMBO_COMPILER_GNU
#else
#   error Unsupported compiler
#endif

#ifdef __cplusplus
#   define ZOMBO_INLINE inline
#else
#   if defined(ZOMBO_COMPILER_MSVC)
#       define ZOMBO_INLINE __forceinline
#   else
#       define ZOMBO_INLINE inline
#   endif
#endif

// ZOMBO_DEBUGBREAK()
#if   defined(ZOMBO_COMPILER_MSVC)
#   define ZOMBO_DEBUGBREAK() __debugbreak()
#elif defined(ZOMBO_COMPILER_GNU) || defined(ZOMBO_COMPILER_CLANG)
#   if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199409L
#       define ZOMBO_DEBUGBREAK() __asm__("int $3")
#   else
#       define ZOMBO_DEBUGBREAK() assert(0)
#   endif
#else
#   error Unsupported compiler
#endif

// Custom assert macro that prints a formatted error message and breaks immediately from the calling code
// - ZOMBO_ASSERT(cond,msg,...): if cond is not true, print msg and assert.
// - ZOMBO_ASSERT_RETURN(cond,retval,msg,...): if cond is not true, print msg and assert, then return retval (for release builds)
// - ZOMBO_ERROR(msg): unconditionally print msg and assert.
#if defined(NDEBUG)
#   define ZOMBO_ASSERT(cond,msg,...) do { (void)( 1 ? (void)0 : (void)(cond) ); } while(0,0)
#   define ZOMBO_ASSERT_RETURN(cond,retval,msg,...) do { if (!(cond)) { return (retval); } } while(0,0)
#elif defined(ZOMBO_COMPILER_MSVC)
#   define ZOMBO_ASSERT(cond,msg,...) \
        __pragma(warning(push)) \
        __pragma(warning(disable:4127)) \
        __pragma(warning(disable:6319)) \
        do { \
            if (!(cond)) { \
                char *zombo_assert_msg_buffer = (char*)malloc(1024); \
                if (zombo_assert_msg_buffer) { \
                    _snprintf_s(zombo_assert_msg_buffer, 1024, 1023, msg ## "\n", __VA_ARGS__); \
                    zombo_assert_msg_buffer[1023] = 0; \
                    OutputDebugStringA(zombo_assert_msg_buffer); \
                    free(zombo_assert_msg_buffer); \
                } \
                IsDebuggerPresent() ? __debugbreak() : assert(cond); \
            } \
        } while(0,0) \
        __pragma(warning(pop))
#   define ZOMBO_ASSERT_RETURN(cond,retval,msg,...) \
        __pragma(warning(push)) \
        __pragma(warning(disable:4127)) \
        __pragma(warning(disable:6319)) \
        do { \
            if (!(cond)) { \
                char *zombo_assert_msg_buffer = (char*)malloc(1024); \
                if (zombo_assert_msg_buffer) { \
                    _snprintf_s(zombo_assert_msg_buffer, 1024, 1023, msg ## "\n", __VA_ARGS__); \
                    zombo_assert_msg_buffer[1023] = 0; \
                    OutputDebugStringA(zombo_assert_msg_buffer); \
                    free(zombo_assert_msg_buffer); \
                } \
                IsDebuggerPresent() ? __debugbreak() : assert(cond); \
                return (retval); \
            } \
        } while(0,0) \
        __pragma(warning(pop))
#elif defined(ZOMBO_COMPILER_GNU) || defined(ZOMBO_COMPILER_CLANG)
#   define ZOMBO_ASSERT(cond,msg,...) \
        do { \
            if (!(cond)) { \
                printf(msg "\n", ## __VA_ARGS__); \
                fflush(stdout); \
                ZOMBO_DEBUGBREAK(); \
            } \
        } while(0)
#   define ZOMBO_ASSERT_RETURN(cond,retval,msg,...) \
        do { \
            if (!(cond)) { \
                printf(msg "\n", ## __VA_ARGS__); \
                fflush(stdout); \
                ZOMBO_DEBUGBREAK(); \
                return (retval); \
            } \
        } while(0)
#else
#   error Unsupported compiler
#endif
#define ZOMBO_ERROR(msg,...) ZOMBO_ASSERT(0, msg, ## __VA_ARGS__)
#define ZOMBO_ERROR_RETURN(retval,msg,...) ZOMBO_ASSERT_RETURN(0, retval, msg, ## __VA_ARGS__)

// ZOMBO_RETVAL_CHECK(expected, expr): if the result of evaluating expr does not equal expected, assert.
#if   defined(ZOMBO_COMPILER_MSVC)
#   define ZOMBO_RETVAL_CHECK(expected, expr) do { \
            int zombo_retval_err = (int)(expr); \
            if (zombo_retval_err != (expected)) { \
                printf("%s(%d): error in %s() -- %s returned %d\n", __FILE__, __LINE__, __FUNCTION__, #expr, zombo_retval_err); \
                ZOMBO_DEBUGBREAK(); \
            } \
            assert(zombo_retval_err == (expected)); \
            __pragma(warning(push)) \
            __pragma(warning(disable:4127)) \
        } while(0) \
        __pragma(warning(pop))
#elif defined(ZOMBO_COMPILER_GNU) || defined(ZOMBO_COMPILER_CLANG)
#   define ZOMBO_RETVAL_CHECK(expected, expr) do { \
            int zombo_retval_err = (int)(expr); \
            if (zombo_retval_err != (expected)) { \
                printf("%s(%d): error in %s() -- %s returned %d\n", __FILE__, __LINE__, __FUNCTION__, #expr, zombo_retval_err); \
                ZOMBO_DEBUGBREAK(); \
            } \
            assert(zombo_retval_err == (expected)); \
        } while(0)
#else
#   error Unsupported compiler
#endif

// popcnt
#if   defined(ZOMBO_COMPILER_MSVC)
#   include <intrin.h>
#   define ZOMBO_POPCNT32(x) __popcnt(x)
#   define ZOMBO_POPCNT64(x) __popcnt64(x)
#elif defined(ZOMBO_COMPILER_CLANG) && defined(ZOMBO_ARCH_X64)
#   include <smmintrin.h>
#   define ZOMBO_POPCNT32(x) _mm_popcnt_u32(x)
#   define ZOMBO_POPCNT64(x) _mm_popcnt_u64(x)
#elif defined(ZOMBO_ARCH_ARM) || defined(ZOMBO_ARCH_ARM64)
#   include <arm_neon.h>
#   define ZOMBO_POPCNT32(x) _CountOneBits(x)
#   define ZOMBO_POPCNT64(x) _CountOneBits64(x) 
#elif defined(ZOMBO_COMPILER_GNU)
// TODO(https://github.com/cdwfs/spokk/issues/7): gcc support
#endif


// zomboAtomic*()
ZOMBO_DEF ZOMBO_INLINE uint32_t zomboAtomicAdd(uint32_t *dest, int32_t val)
{
#if   defined(ZOMBO_COMPILER_MSVC)
    return InterlockedAdd((LONG*)dest, (LONG)val);
#elif defined(ZOMBO_COMPILER_GNU) || defined(ZOMBO_COMPILER_CLANG)
    return __sync_fetch_and_add(dest, val);
#else
#   error Unsupported compiler
#endif
}

// zombo*nprintf()
#if   defined(ZOMBO_OS_WINDOWS)
#   define zomboSnprintf( str, size, fmt, ...)  _snprintf((str), (size), (fmt), ## __VA_ARGS__)
#   define zomboVsnprintf(str, size, fmt, ap)   _vsnprintf((str), (size), (fmt), (ap)
#   define zomboScanf(format, ...)              scanf_s((format), __VA_ARGS__)
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
#   define zomboSnprintf( str, size, fmt, ...)  snprintf((str), (size), (fmt), ## __VA_ARGS__)
#   define zomboVsnprintf(str, size, fmt, ap)   vsnprintf((str), (size), (fmt), (ap)
#   define zomboScanf(format, ...)              scanf((format), __VA_ARGS__)
#endif

// zomboStr*()
#if   defined(ZOMBO_OS_WINDOWS)
#   define zomboStrcasecmp(s1, s2)      _stricmp( (s1), (s2) )
#   define zomboStrncasecmp(s1, s2, n)  _strnicmp( (s1), (s2), (n) )
#   define zomboStrncpy(dest, src, n)   strncpy_s( (dest), (n), (src), (n) )
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
#   define zomboStrcasecmp(s1, s2)      strcasecmp( (s1), (s2) )
#   define zomboStrncasecmp(s1, s2, n)  strncasecmp( (s1), (s2), (n) )
#   define zomboStrncpy(dest, src, n)   strncpy( (dest), (src), (n) )
#endif

#if   defined(ZOMBO_OS_WINDOWS)
#   define zomboChdir(dir)             _chdir( (dir) )
#   define zomboMkdir(dir)             _mkdir( (dir) )
#   define zomboGetcwd(buf, size)      _getcwd( (buf), (size) )
typedef struct _stat ZomboStatStruct;
#   define zomboStat(path, pstat)       _stat( (path), (pstat) )
#elif defined(ZOMBO_OS_APPLE) || defined(ZOMBO_OS_POSIX)
#   define zomboChdir(dir)             chdir( (dir) )
#   define zomboMkdir(dir)             mkdir( (dir), 0755 )
#   define zomboGetcwd(buf, size)      getcwd( (buf), (size) )
typedef struct stat ZomboStatStruct;
#   define zomboStat(path, pstat)       stat( (path), (pstat) )
#endif

ZOMBO_DEF int32_t zomboCpuCount(void);
ZOMBO_DEF uint64_t zomboClockTicks(void);
ZOMBO_DEF double zomboTicksToSeconds(uint64_t ticks);
ZOMBO_DEF int zomboProcessId(void);
ZOMBO_DEF int zomboThreadId(void);
ZOMBO_DEF void zomboSleepMsec(uint32_t msec);
ZOMBO_DEF FILE *zomboFopen(const char *path, const char *mode);
ZOMBO_DEF char* zomboGetEnv(const char *varname);

#ifdef __cplusplus
}
#endif
// clang-format on

#endif  // !defined(SPOKK_PLATFORM_H)
