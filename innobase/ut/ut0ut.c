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
#include <string.h>

#include "ut0sort.h"
#include "trx0trx.h"

ibool	ut_always_false	= FALSE;

/*********************************************************************
Get the quote character to be used in SQL identifiers.
This definition must match the one in sql/ha_innodb.cc! */
extern
int
mysql_get_identifier_quote_char(
/*============================*/
				/* out: quote character to be
				used in SQL identifiers; EOF if none */
	trx_t*		trx,	/* in: transaction */
	const char*	name,	/* in: name to print */
	ulint		namelen);/* in: length of name */

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
The following function returns elapsed CPU time in milliseconds. */

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
Sprintfs a timestamp to a buffer, 13..14 chars plus terminating NUL. */

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
Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'. */

void
ut_sprintf_timestamp_without_extra_chars(
/*=====================================*/
	char*	buf) /* in: buffer where to sprintf */
{
#ifdef __WIN__
  	SYSTEMTIME cal_tm;

  	GetLocalTime(&cal_tm);

  	sprintf(buf, "%02d%02d%02d_%2d_%02d_%02d",
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
  	sprintf(buf, "%02d%02d%02d_%2d_%02d_%02d",
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
		ut_always_false = (ibool) j;
	}
	
	return(j);
}	

/*****************************************************************
Prints the contents of a memory buffer in hex and ascii. */

void
ut_print_buf(
/*=========*/
	FILE*		file,	/* in: file where to print */
	const byte*	buf,	/* in: memory buffer */
	ulint		len)	/* in: length of the buffer */
{
	const byte*	data;
	ulint		i;

	fprintf(file, " len %lu; hex ", len);

	for (data = buf, i = 0; i < len; i++) {
		fprintf(file, "%02lx", (ulong)*data++);
	}

	fputs("; asc ", file);

	data = buf;

	for (i = 0; i < len; i++) {
		int	c = (int) *data++;
		putc(isprint(c) ? c : ' ', file);
	}

	putc(';', file);
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

/**************************************************************************
Outputs a NUL-terminated file name, quoted with apostrophes. */

void
ut_print_filename(
/*==============*/
	FILE*		f,	/* in: output stream */
	const char*	name)	/* in: name to print */
{
	putc('\'', f);
	for (;;) {
		int	c = *name++;
		switch (c) {
		case 0:
			goto done;
		case '\'':
			putc(c, f);
			/* fall through */
		default:
			putc(c, f);
		}
	}
done:
	putc('\'', f);
}

/**************************************************************************
Outputs a NUL-terminated string, quoted as an SQL identifier. */

void
ut_print_name(
/*==========*/
	FILE*		f,	/* in: output stream */
	trx_t*		trx,	/* in: transaction */
	const char*	name)	/* in: name to print */
{
	ut_print_namel(f, trx, name, strlen(name));
}

/**************************************************************************
Outputs a fixed-length string, quoted as an SQL identifier. */

void
ut_print_namel(
/*==========*/
	FILE*		f,	/* in: output stream */
	trx_t*		trx,	/* in: transaction (NULL=no quotes) */
	const char*	name,	/* in: name to print */
	ulint		namelen)/* in: length of name */
{
	const char*	s = name;
	const char*	e = s + namelen;
	int		q = mysql_get_identifier_quote_char(trx, name, namelen);
	if (q == EOF) {
		fwrite(name, 1, namelen, f);
		return;
	}
	putc(q, f);
	while (s < e) {
		int	c = *s++;
		if (c == q) {
			putc(c, f);
		}
		putc(c, f);
	}
	putc(q, f);
}

/**************************************************************************
Catenate files. */

void
ut_copy_file(
/*=========*/
	FILE*	dest,	/* in: output file */
	FILE*	src)	/* in: input file to be appended to output */
{
	long	len = ftell(src);
	char	buf[4096];

	rewind(src);
	do {
		size_t	maxs =
			len < (long) sizeof buf ? (size_t) len : sizeof buf;
		size_t	size = fread(buf, 1, maxs, src);
		fwrite(buf, 1, size, dest);
		len -= size;
		if (size < maxs) {
			break;
		}
	} while (len > 0);
}
