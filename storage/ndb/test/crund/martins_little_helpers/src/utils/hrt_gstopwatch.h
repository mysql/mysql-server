/*
 * hrt_gstopwatch.h
 *
 */

#ifndef _utils_hrt_gstopwatch
#define _utils_hrt_gstopwatch
#ifdef __cplusplus
extern "C" {
#endif

#include "hrt_stopwatch.h"

/*
 * High-Resolution Time Global Stopwatch Utility
 */

//extern hrt_stopwatch gsw;

/**
 * Initializes the global stopwatch with a given storage capacity for timemarks.
 */
extern void hrt_gsw_init(int cap);

/**
 * Releases all reources allocated by the global stopwatch.
 */
extern void hrt_gsw_close(void);

/**
 * Returns the index of the global stopwatch's last timemark.
 */
extern int hrt_gsw_top(void);

/**
 * Returns the number of timemarks the global stopwatch can hold.
 */
extern int hrt_gsw_capacity(void);

/**
 * Marks the time and stores that mark in the global stopwatch returning the index.
 */
extern int hrt_gsw_pushmark(void);

/**
 * Returns the real time amount between two timemarks in microseonds
 * (i.e., y - x).
 */
extern double hrt_gsw_rtmicros(int y, int x);

/**
 * Returns the cpu time amount between two timemarks in microseonds
 * (i.e., y - x).
 */
extern double hrt_gsw_ctmicros(int y, int x);

/**
 * Clears all timemarks stored in the global stopwatch.
 */
extern void hrt_gsw_clear(void);

#ifdef __cplusplus
}
#endif
#endif
