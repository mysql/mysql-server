/*
 * Public domain
 * sys/time.h compatibility shim
 */

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#include <../ucrt/time.h>
#elif defined(_MSC_VER) && (_MSC_VER < 1900)
#include <../include/time.h>
#else
#include <time.h>
#endif

#ifndef _COMPAT_TIME_H
#define _COMPAT_TIME_H

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

#ifndef HAVE_CLOCK_GETTIME
typedef int clockid_t;
int clock_gettime(clockid_t, struct timespec *);
#endif

#ifdef HAVE_TIMESPECSUB
#include <sys/time.h>
#endif

#ifndef HAVE_TIMESPECSUB
#define	timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (0)

#define timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)

#define	timespeccmp(tsp, usp, cmp)					\
	(((tsp)->tv_sec == (usp)->tv_sec) ?				\
	    ((tsp)->tv_nsec cmp (usp)->tv_nsec) :			\
	    ((tsp)->tv_sec cmp (usp)->tv_sec))
#endif

#endif /* _COMPAT_TIME_H */
