/*******************************************************************
Various utilities for Innobase.

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "ut0ut.h"

#ifdef UNIV_NONINL
#include "ut0ut.ic"
#endif

#include "ut0sort.h"

ibool	ut_always_false	= FALSE;

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
  struct tm* cal_tm_ptr;
  struct tm  cal_tm;
  time_t     tm;

 try_again:
  time(&tm);

  cal_tm_ptr = localtime(&tm);

  memcpy(&cal_tm, cal_tm_ptr, sizeof(struct tm));

  /* In theory localtime may return a wrong result because its return
  struct is not protected with a mutex */

  if (difftime(tm, mktime(&cal_tm)) > 0.5
      || difftime(tm, mktime(&cal_tm)) < -0.5) {

    goto try_again;
  }

  fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
	  cal_tm.tm_year % 100,
	  cal_tm.tm_mon+1,
	  cal_tm.tm_mday,
	  cal_tm.tm_hour,
	  cal_tm.tm_min,
	  cal_tm.tm_sec);
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
		if (isprint((char)(*data))) {
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
		if (isprint((char)(*data))) {
			n += sprintf(str + n, "%c", (char)*data);
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
