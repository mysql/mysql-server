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

#ifdef __cplusplus
};
#endif

#endif
