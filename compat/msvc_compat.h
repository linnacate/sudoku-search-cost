/*  msvc_compat.h — MSVC(Windows) 编译兼容垫片
 *
 *  用途：本包的源码按 POSIX/Linux 编写（gcc），其中只用到一处 POSIX 专有 API
 *        —— clock_gettime(CLOCK_MONOTONIC, ...)（仅用于墙钟计时）。
 *        在 Windows/MSVC 下用 /FIcompat\msvc_compat.h 强制包含本文件即可编译，
 *        无需改动任何原始源码。
 *
 *  用法（在开发者命令提示符 / vcvars64 之后）:
 *      cl /nologo /O2 /utf-8 /FIcompat\msvc_compat.h /I core ^
 *         /Fe:build_win\exact_bench.exe tools\exact_bench.c /link
 *
 *  注意：本垫片只影响**墙钟数字**（Windows 与 Linux 不可比，论文已声明墙钟不作跨机依据）；
 *        它不触碰任何搜索/决策逻辑，故 guesses 等机器无关量应与 Linux/gcc 构建逐位一致。
 *        本项目随包的 bin/ 与论文数字均为 Linux/gcc 构建产物。
 */
#ifndef SUDOKU_FUZE_MSVC_COMPAT_H
#define SUDOKU_FUZE_MSVC_COMPAT_H

#if defined(_MSC_VER)

#include <time.h>
#include <windows.h>
#include <intrin.h>

/* ---- GCC 内建位运算 → MSVC 等价实现（语义一致，纯整数，无浮点差异）----
 * __builtin_ctz(x)       = 尾部 0 个数；MSVC: _BitScanForward（x=0 时未定义，GCC 同）
 * __builtin_popcount(x)  = 二进制中 1 的个数
 */
#ifndef __builtin_ctz
static __inline int sudoku_fuze_ctz(unsigned int x)
{
    unsigned long idx;
    if (x == 0) return 32;            /* 与 GCC 在 x=0 时"未定义"不同：这里给确定值 */
    _BitScanForward(&idx, x);
    return (int)idx;
}
#define __builtin_ctz sudoku_fuze_ctz
#endif

#ifndef __builtin_popcount
static __inline int sudoku_fuze_popcount(unsigned int x)
{
    /* SWAR 位计数：不依赖 CPU 的 POPCNT 指令，任何 x86 都能跑 */
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (int)((x * 0x01010101u) >> 24);
}
#define __builtin_popcount(x) sudoku_fuze_popcount((unsigned int)(x))
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* 默认在 MSVC 下改用 QueryPerformanceCounter：
 * 不依赖 UCRT 是否导出 clock_gettime（VS2022 17.10+ 才有），行为确定、可复现。 */
#ifndef SUDOKU_FUZE_USE_QPC
#define SUDOKU_FUZE_USE_QPC 1
#endif

#if SUDOKU_FUZE_USE_QPC
static __inline int sudoku_fuze_clock_gettime(int which, struct timespec *ts)
{
    static LARGE_INTEGER freq;
    static int inited = 0;
    LARGE_INTEGER now;
    (void)which;
    if (!inited) { QueryPerformanceFrequency(&freq); inited = 1; }
    QueryPerformanceCounter(&now);
    if (freq.QuadPart <= 0) { ts->tv_sec = 0; ts->tv_nsec = 0; return 0; }
    ts->tv_sec  = (time_t)(now.QuadPart / freq.QuadPart);
    ts->tv_nsec = (long)(((now.QuadPart % freq.QuadPart) * 1000000000LL) / freq.QuadPart);
    return 0;
}
#define clock_gettime sudoku_fuze_clock_gettime
#endif /* SUDOKU_FUZE_USE_QPC */

#endif /* _MSC_VER */

/* ---- gettimeofday ---- 
 * sp2.c / sp3.c 直接调用 gettimeofday() 但**没有** #include <sys/time.h>，
 * 故这里无条件提供一份（与 compat/sys/time.h 中的实现互斥保护）。
 */
#if !defined(SUDOKU_FUZE_HAVE_GETTIMEOFDAY)
#define SUDOKU_FUZE_HAVE_GETTIMEOFDAY 1
#if !defined(_TIMEVAL_DEFINED) && !defined(_WINSOCKAPI_) && !defined(_WIN32_WINNT_TIMEVAL_)
struct timeval { long tv_sec; long tv_usec; };
#define _TIMEVAL_DEFINED
#endif
static __inline int sudoku_fuze_gettimeofday(struct timeval *tv, void *tz)
{
    static LARGE_INTEGER qfreq;
    static int qinit = 0;
    LARGE_INTEGER qnow;
    (void)tz;
    if (!qinit) { QueryPerformanceFrequency(&qfreq); qinit = 1; }
    QueryPerformanceCounter(&qnow);
    if (qfreq.QuadPart <= 0) { tv->tv_sec = 0; tv->tv_usec = 0; return 0; }
    tv->tv_sec  = (long)(qnow.QuadPart / qfreq.QuadPart);
    tv->tv_usec = (long)(((qnow.QuadPart % qfreq.QuadPart) * 1000000LL) / qfreq.QuadPart);
    return 0;
}
#define gettimeofday sudoku_fuze_gettimeofday
#endif
#endif /* SUDOKU_FUZE_MSVC_COMPAT_H */
