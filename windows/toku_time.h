#ifndef TOKU_TIME_H
#define TOKU_TIME_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz);

#ifdef __cplusplus
};
#endif

#endif
