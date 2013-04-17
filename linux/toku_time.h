#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#ifndef TOKU_TIME_H
#define TOKU_TIME_H

#include <time.h>
#include <sys/time.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

static inline float toku_tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
