#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
typedef pthread_size_t size_t;
#endif 

#ifndef _CLOCK_T
#define _CLOCK_T
typedef pthread_clock_t	clock_t;
#endif 

#ifndef _TIME_T
#define _TIME_T
typedef pthread_time_t time_t;
#endif /* !_TIME_T */

#define CLOCKS_PER_SEC		1000000

#ifndef CLK_TCK
#define CLK_TCK	sysconf(3)	/* clock ticks per second */
				/* 3 is _SC_CLK_TCK */
#endif
