#include <pthread/types.h>
#include <machine/machtime.h> /* CLOCKS_PER_SEC is defined here */

#ifndef _SIZE_T
#define _SIZE_T
typedef pthread_size_t size_t;
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef pthread_clock_t clock_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef pthread_time_t time_t;
#endif

#ifndef CLK_TCK
#define CLK_TCK 60
#endif
