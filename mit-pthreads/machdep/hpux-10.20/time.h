/* $Header$ */

#ifndef _SYS_TIME_INCLUDED
#define _SYS_TIME_INCLUDED

/* time.h: Definitions for time handling functions */

#ifdef _KERNEL_BUILD
#include "../h/stdsyms.h"
#else  /* ! _KERNEL_BUILD */
#include <sys/stdsyms.h>
#endif /* _KERNEL_BUILD */

#include <sys/types.h>

/* ANSI C time constants, types, and structures */

#ifdef _INCLUDE__STDC__
#  define CLOCKS_PER_SEC 1000000

#  ifndef NULL
#    define NULL 0
#  endif

#  ifndef _CLOCK_T
#    define _CLOCK_T
     typedef unsigned long clock_t;
#  endif /* _CLOCK_T */

#  ifndef _TIME_T
#    define _TIME_T
     typedef long time_t;
#  endif /* _TIME_T */

#  ifndef _SIZE_T
#    define _SIZE_T
     typedef unsigned int size_t;
#  endif /* _SIZE_T */

   /* Structure used with gmtime(), localtime(), mktime(), strftime(). */
   struct tm {
      int tm_sec;	/* second (0-61, allows for leap seconds) */
      int tm_min;	/* minute (0-59) */
      int tm_hour;	/* hour (0-23) */
      int tm_mday;	/* day of the month (1-31) */
      int tm_mon;	/* month (0-11) */
      int tm_year;	/* years since 1900 */
      int tm_wday;	/* day of the week (0-6) */
      int tm_yday;	/* day of the year (0-365) */
      int tm_isdst;	/* non-0 if daylight savings time is in effect */
   };
#endif /* _INCLUDE__STDC__ */


/* Additional types needed for HP-UX */

#ifdef _INCLUDE_HPUX_SOURCE
# ifndef _STRUCT_TIMEVAL
#  define _STRUCT_TIMEVAL
   /* Structure returned by gettimeofday(2) system call and others */
     struct timeval {
	  unsigned long	tv_sec;		/* seconds */
	  long		tv_usec;	/* and microseconds */
     };
# endif /* _STRUCT_TIMEVAL */

   /* Structure used to represent timezones for gettimeofday(2) and others */
   struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
   };

   /* Structure defining a timer setting.  */
   struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
   };
#endif /* _INCLUDE_HPUX_SOURCE */


/* Function prototypes and external variable declarations */

#ifndef _KERNEL
#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

#ifdef _INCLUDE__STDC__
#  ifdef _PROTOTYPES
     extern double difftime(time_t, time_t);
     extern time_t mktime(struct tm *);
     extern time_t time(time_t *);
     extern char *ctime(const time_t *);
     extern struct tm *gmtime(const time_t *);
     extern struct tm *localtime(const time_t *);
     extern size_t strftime(char *, size_t, const char *, const struct tm *);
#  else /* not _PROTOTYPES */
     extern double difftime();
     extern time_t mktime();
     extern time_t time();
     extern char *ctime();
     extern struct tm *gmtime();
     extern struct tm *localtime();
     extern size_t strftime();
#  endif /* not _PROTOTYPES */

#  ifdef _CLASSIC_ANSI_TYPES
     extern long clock();
#  else /* not _CLASSIC_ANSI_TYPES */
#    ifdef _PROTOTYPES
       extern clock_t clock(void);
#    else /* not _PROTOTYPES */
       extern clock_t clock();
#    endif /* not _PROTOTYPES */
#  endif /* not _CLASSIC_ANSI_TYPES */
#endif /* _INCLUDE__STDC__ */

#ifdef _INCLUDE_POSIX_SOURCE
#  ifdef _PROTOTYPES
     extern void tzset(void);
#  else /* not _PROTOTYPES */
     extern void tzset();
#  endif /* not _PROTOTYPES */

   extern char *tzname[2];
#endif /* _INCLUDE_POSIX_SOURCE */


#ifdef _INCLUDE_XOPEN_SOURCE
#  ifdef _PROTOTYPES
     extern char *strptime(const char *, const char *, struct tm *);
#  else /* not _PROTOTYPES */
     extern char *strptime();
#  endif /* not _PROTOTYPES */

   extern long timezone;
   extern int daylight;
#endif /* _INCLUDE_XOPEN_SOURCE */


#ifdef _INCLUDE_HPUX_SOURCE
#  ifdef _PROTOTYPES
     extern struct tm *getdate(const char *);
     extern char *nl_asctime(struct tm *, char *, int);
     extern char *nl_ctime(long *, char *, int);
     extern char *nl_ascxtime(struct tm *, char *);
     extern char *nl_cxtime(long *, char *);
     extern int getitimer(int, struct itimerval *);
     extern int setitimer(int, const struct itimerval *, struct itimerval *);
     extern int gettimeofday(struct timeval *, struct timezone *);
     extern int settimeofday(const struct timeval *, const struct timezone *);
     extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
     extern int stime(const time_t *);
#  else /* not _PROTOTYPES */
     extern struct tm *getdate();
     extern char *nl_asctime();
     extern char *nl_ctime();
     extern char *nl_ascxtime();
     extern char *nl_cxtime();
     extern int getitimer();
     extern int setitimer();
     extern int gettimeofday();
     extern int settimeofday();
     extern int select();
     extern int stime();
#  endif /* not _PROTOTYPES */
     extern int getdate_err;
#endif /* _INCLUDE_HPUX_SOURCE */

#ifdef __cplusplus
   }
#endif /* __cplusplus */
#endif /* not _KERNEL */


/*
 * CLK_TCK is needed by the kernel, and also in the POSIX namespace.
 */

#ifdef _INCLUDE_POSIX_SOURCE
#  ifndef CLK_TCK
#    ifdef __hp9000s300
#      define CLK_TCK 50
#    endif /* __hp9000s300 */
#    ifdef __hp9000s800
#      define CLK_TCK 100
#    endif /* __hp9000s800 */
#  endif /* CLK_TCK */
#endif


/* Additional HP-UX structures, macros, and constants */

#ifdef _INCLUDE_HPUX_SOURCE

   /* Kernel instrumentation time value */
    struct	ki_timeval {
	    long	tv_sec;		/* seconds */
	    long	tv_nunit;	/* and native units */
    };

   /* Kinds of daylight savings time */
#  define DST_NONE	0	/* not on dst */
#  define DST_USA	1	/* USA style dst */
#  define DST_AUST	2	/* Australian style dst */
#  define DST_WET	3	/* Western European dst */
#  define DST_MET	4	/* Middle European dst */
#  define DST_EET	5	/* Eastern European dst */

   /*
    * Operations on timevals.
    *
    * NB: timercmp does not work for >= or <=.
    */
#  define timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#  define timercmp(tvp, uvp, cmp) \
          ((tvp)->tv_sec cmp (uvp)->tv_sec || \
           (tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#  define timerclear(tvp)		((tvp)->tv_sec = (tvp)->tv_usec = 0)

   /* Names of the interval timers */
#  define ITIMER_REAL		0
#  define ITIMER_VIRTUAL	1
#  define ITIMER_PROF		2

#endif /* _INCLUDE_HPUX_SOURCE */

#endif /* _SYS_TIME_INCLUDED */
