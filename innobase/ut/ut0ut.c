/*******************************************************************
Various utilities for Innobase.

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "ut0ut.h"

#ifdef UNIV_NONINL
#include "ut0ut.ic"
#endif

#include <stdarg.h>

#include "ut0sort.h"

ibool	ut_always_false	= FALSE;

/************************************************************
Uses vsprintf to emulate sprintf so that the function always returns
the printed length. Apparently in some old SCO Unixes sprintf did not
return the printed length but a pointer to the end of the printed string. */

ulint
ut_sprintf(
/*=======*/
        char*       buf,     /* in/out: buffer where to print */
        const char* format,  /* in: format of prints */
        ...)                 /* in: arguments to be printed */
{
        va_list   args;
  
        va_start(args, format);

        vsprintf(buf, format, args);

        va_end(args);

        return((ulint)strlen(buf));
}

/************************************************************
Gets the high 32 bits in a ulint. That is makes a shift >> 32,
but since there seem to be compiler bugs in both gcc and Visual C++,
we do this by a special conversion. */

ulint
ut_get_high32(
/*==========*/
			/* out: a >> 32 */
	ulint	a)	/* in: ulint */
{
#if SIZEOF_LONG == 4
	UT_NOT_USED(a);

	return 0;
#else
	return(a >> 32);
#endif
}

/************************************************************
The following function returns a clock time in milliseconds. */

ulint
ut_clock(void)
{
	return((clock() * 1000) / CLOCKS_PER_SEC);
}

/**************************************************************
Returns system time. We do not specify the format of the time returned:
the only way to manipulate it is to use the function ut_difftime. */

ib_time_t
ut_time(void)
/*=========*/
{
	return(time(NULL));
}

/**************************************************************
Returns the difference of two times in seconds. */

double
ut_difftime(
/*========*/
				/* out: time2 - time1 expressed in seconds */
	ib_time_t	time2,	/* in: time */
	ib_time_t	time1)	/* in: time */
{
	return(difftime(time2, time1));
}

/**************************************************************
Prints a timestamp to a file. */

void
ut_print_timestamp(
/*===============*/
	FILE*  file) /* in: file where to print */
{
#ifdef __WIN__
  	SYSTEMTIME cal_tm;

  	GetLocalTime(&cal_tm);

  	fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
	  (int)cal_tm.wYear % 100,
	  (int)cal_tm.wMonth,
	  (int)cal_tm.wDay,
	  (int)cal_tm.wHour,
	  (int)cal_tm.wMinute,
	  (int)cal_tm.wSecond);
#else
	struct tm  cal_tm;
  	struct tm* cal_tm_ptr;
  	time_t     tm;

  	time(&tm);

#ifdef HAVE_LOCALTIME_R
  	localtime_r(&tm, &cal_tm);
  	cal_tm_ptr = &cal_tm;
#else
  	cal_tm_ptr = localtime(&tm);
#endif
  	fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
	  cal_tm_ptr->tm_year % 100,
	  cal_tm_ptr->tm_mon + 1,
	  cal_tm_ptr->tm_mday,
	  cal_tm_ptr->tm_hour,
	  cal_tm_ptr->tm_min,
	  cal_tm_ptr->tm_sec);
#endif
}

/**************************************************************
Sprintfs a timestamp to a buffer. */

void
ut_sprintf_timestamp(
/*=================*/
	char*	buf) /* in: buffer where to sprintf */
{
#ifdef __WIN__
  	SYSTEMTIME cal_tm;

  	GetLocalTime(&cal_tm);

  	sprintf(buf, "%02d%02d%02d %2d:%02d:%02d",
	  (int)cal_tm.wYear % 100,
	  (int)cal_tm.wMonth,
	  (int)cal_tm.wDay,
	  (int)cal_tm.wHour,
	  (int)cal_tm.wMinute,
	  (int)cal_tm.wSecond);
#else
	struct tm  cal_tm;
  	struct tm* cal_tm_ptr;
  	time_t     tm;

  	time(&tm);

#ifdef HAVE_LOCALTIME_R
  	localtime_r(&tm, &cal_tm);
  	cal_tm_ptr = &cal_tm;
#else
  	cal_tm_ptr = localtime(&tm);
#endif
  	sprintf(buf, "%02d%02d%02d %2d:%02d:%02d",
	  cal_tm_ptr->tm_year % 100,
	  cal_tm_ptr->tm_mon + 1,
	  cal_tm_ptr->tm_mday,
	  cal_tm_ptr->tm_hour,
	  cal_tm_ptr->tm_min,
	  cal_tm_ptr->tm_sec);
#endif
}

/**************************************************************
Returns current year, month, day. */

void
ut_get_year_month_day(
/*==================*/
	ulint*	year,	/* out: current year */
	ulint*	month,	/* out: month */
	ulint*	day)	/* out: day */
{
#ifdef __WIN__
  	SYSTEMTIME cal_tm;

  	GetLocalTime(&cal_tm);

  	*year = (ulint)cal_tm.wYear;
  	*month = (ulint)cal_tm.wMonth;
  	*day = (ulint)cal_tm.wDay;
#else
  	struct tm* cal_tm_ptr;
  	time_t     tm;

  	time(&tm);

  	cal_tm_ptr = localtime(&tm);

  	*year = (ulint)cal_tm_ptr->tm_year + 1900;
  	*month = (ulint)cal_tm_ptr->tm_mon + 1;
  	*day = (ulint)cal_tm_ptr->tm_mday;
#endif
}

/*****************************************************************
Runs an idle loop on CPU. The argument gives the desired delay
in microseconds on 100 MHz Pentium + Visual C++. */

ulint
ut_delay(
/*=====*/
			/* out: dummy value */
	ulint	delay)	/* in: delay in microseconds on 100 MHz Pentium */
{
	ulint	i, j;

	j = 0;

	for (i = 0; i < delay * 50; i++) {
		j += i;
	}

	if (ut_always_false) {
		printf("%lu", j);
	}
	
	return(j);
}	

/*****************************************************************
Prints the contents of a memory buffer in hex and ascii. */

void
ut_print_buf(
/*=========*/
	byte*	buf,	/* in: memory buffer */
	ulint 	len)	/* in: length of the buffer */
{
	byte*	data;
	ulint	i;

	printf(" len %lu; hex ", len);
			
	data = buf;

	for (i = 0; i < len; i++) {
		printf("%02lx", (ulint)*data);
		data++;
	}

	printf("; asc ");

	data = buf;

	for (i = 0; i < len; i++) {
		if (isprint((int)(*data))) {
			printf("%c", (char)*data);
		}
		data++;
	}

	printf(";");
}

/*****************************************************************
Prints the contents of a memory buffer in hex and ascii. */

ulint
ut_sprintf_buf(
/*===========*/
			/* out: printed length in bytes */
	char*	str,	/* in: buffer to print to */
	byte*	buf,	/* in: memory buffer */
	ulint 	len)	/* in: length of the buffer */
{
	byte*	data;
	ulint	n;
	ulint	i;

	n = 0;
	
	n += sprintf(str + n, " len %lu; hex ", len);
			
	data = buf;

	for (i = 0; i < len; i++) {
		n += sprintf(str + n, "%02lx", (ulint)*data);
		data++;
	}

	n += sprintf(str + n, "; asc ");

	data = buf;

	for (i = 0; i < len; i++) {
		if (isprint((int)(*data))) {
			n += sprintf(str + n, "%c", (char)*data);
		} else {
			n += sprintf(str + n, ".");
		}
		
		data++;
	}

	n += sprintf(str + n, ";");

	return(n);
}

/****************************************************************
Sort function for ulint arrays. */

void
ut_ulint_sort(ulint* arr, ulint* aux_arr, ulint low, ulint high)
/*============================================================*/
{
	UT_SORT_FUNCTION_BODY(ut_ulint_sort, arr, aux_arr, low, high,
								ut_ulint_cmp);
}

/*****************************************************************
Calculates fast the number rounded up to the nearest power of 2. */

ulint
ut_2_power_up(
/*==========*/
			/* out: first power of 2 which is >= n */
	ulint	n)	/* in: number != 0 */
{
	ulint	res;

	res = 1;

	ut_ad(n > 0);

	while (res < n) {
		res = res * 2;
	}

	return(res);
}

