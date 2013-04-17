#ifndef TOKU_TIME_H
#define TOKU_TIME_H

#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline float toku_tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}
#ifdef __cplusplus
};
#endif

#endif
