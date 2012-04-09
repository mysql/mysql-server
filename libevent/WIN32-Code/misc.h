#ifndef MISC_H
#define MISC_H

struct timezone;
struct timeval;

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval *,struct timezone *);
#endif

#endif
