#ifndef TOKU_TIME_H
#define TOKU_TIME_H

#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

typedef enum {
    CLOCK_REALTIME = 0
} clockid_t;

struct timespec {
    long tv_sec;
    long tv_nsec;
};

int clock_gettime(clockid_t clock_id, struct timespec *ts);

#ifdef __cplusplus
};
#endif

#endif
