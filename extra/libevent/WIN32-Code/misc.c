#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <sys/timeb.h>
#include <time.h>

#ifdef __GNUC__
/*our prototypes for timeval and timezone are in here, just in case the above
  headers don't have them*/
#include "misc.h"
#endif

/****************************************************************************
 *
 * Function: gettimeofday(struct timeval *, struct timezone *)
 *
 * Purpose:  Get current time of day.
 *
 * Arguments: tv => Place to store the curent time of day.
 *            tz => Ignored.
 *
 * Returns: 0 => Success.
 *
 ****************************************************************************/

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval *tv, struct timezone *tz) {
	struct _timeb tb;

	if(tv == NULL)
		return -1;

	_ftime(&tb);
	tv->tv_sec = (long) tb.time;
	tv->tv_usec = ((int) tb.millitm) * 1000;
	return 0;
}
#endif
