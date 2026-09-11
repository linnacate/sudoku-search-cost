/*  sys/time.h — MSVC(Windows) 兼容垫片
 *
 *  本包源码按 POSIX 编写，用到 gettimeofday()（仅计时）。
 *  MSVC 没有 <sys/time.h>，故在 compat/ 下提供同名头文件：
 *  构建时加 -I compat，<sys/time.h> 会优先命中本文件。
 *
 *  只影响墙钟；不触碰任何搜索/决策逻辑。
 */
#ifndef SUDOKU_FUZE_COMPAT_SYS_TIME_H
#define SUDOKU_FUZE_COMPAT_SYS_TIME_H

#if defined(_MSC_VER)

#include <time.h>
#include <windows.h>

/* Windows SDK（winsock2.h 等）已经定义过 struct timeval，必须让开，否则 C2011 重定义 */
#if !defined(_TIMEVAL_DEFINED) && !defined(_WINSOCKAPI_) && !defined(_WIN32_WINNT_TIMEVAL_)
struct timeval {
    long tv_sec;
    long tv_usec;
};
#define _TIMEVAL_DEFINED
#endif

#ifndef SUDOKU_FUZE_HAVE_GETTIMEOFDAY
#define SUDOKU_FUZE_HAVE_GETTIMEOFDAY 1
static __inline int gettimeofday(struct timeval *tv, void *tz)
{
    static LARGE_INTEGER freq;
    static int inited = 0;
    LARGE_INTEGER now;
    (void)tz;
    if (!inited) { QueryPerformanceFrequency(&freq); inited = 1; }
    QueryPerformanceCounter(&now);
    if (freq.QuadPart <= 0) { tv->tv_sec = 0; tv->tv_usec = 0; return 0; }
    tv->tv_sec  = (long)(now.QuadPart / freq.QuadPart);
    tv->tv_usec = (long)(((now.QuadPart % freq.QuadPart) * 1000000LL) / freq.QuadPart);
    return 0;
}
#endif

#else
#include_next <sys/time.h>
#endif /* _MSC_VER */
#endif /* SUDOKU_FUZE_COMPAT_SYS_TIME_H */
