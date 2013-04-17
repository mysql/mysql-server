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

typedef struct toku_timespec_struct {
    long tv_sec;
    long tv_nsec;
} toku_timespec_t;

int clock_gettime(clockid_t clock_id, toku_timespec_t *ts);

static inline float toku_tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

char * ctime_r(const time_t *timep, char *buf);

#ifdef __cplusplus
};
#endif

#endif
