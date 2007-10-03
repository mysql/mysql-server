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
#include <ctype.h>

#include "ut0sort.h"
#include "trx0trx.h"
#include "ha_prototypes.h"

ibool	ut_always_false	= FALSE;

#ifdef __WIN__
/*********************************************************************
NOTE: The Windows epoch starts from 1601/01/01 whereas the Unix
epoch starts from 1970/1/1. For selection of constant see:
http://support.microsoft.com/kb/167296/ */
#define WIN_TO_UNIX_DELTA_USEC  ((ib_longlong) 11644473600000000ULL)


/*********************************************************************
This is the Windows version of gettimeofday(2).*/
static
int
ut_gettimeofday(
/*============*/
			/* out: 0 if all OK else -1 */
	struct timeval*	tv,	/* out: Values are relative to Unix epoch */
	void*		tz)	/* in: not used */
{
	FILETIME	ft;
	ib_longlong	tm;

	if (!tv) {
		errno = EINVAL;
		return(-1);
	}

	GetSystemTimeAsFileTime(&ft);

	tm = (ib_longlong) ft.dwHighDateTime << 32;
	tm |= ft.dwLowDateTime;

	ut_a(tm >= 0);	/* If tm wraps over to negative, the quotient / 10
			does not work */

	tm /= 10;	/* Convert from 100 nsec periods to usec */

	/* If we don't convert to the Unix epoch the value for
	struct timeval::tv_sec will overflow.*/
	tm -= WIN_TO_UNIX_DELTA_USEC;

	tv->tv_sec  = (long) (tm / 1000000L);
	tv->tv_usec = (long) (tm % 1000000L);

	return(0);
}
#else
#define	ut_gettimeofday		gettimeofday
#endif

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
	ib_longlong	i;

	i = (ib_longlong)a;

	i = i >> 32;

	return((ulint)i);
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
Returns system time. */

void
ut_usectime(
/*========*/
	ulint*	sec,	/* out: seconds since the Epoch */
	ulint*	ms)	/* out: microseconds since the Epoch+*sec */
{
	struct timeval	tv;

	ut_gettimeofday(&tv, NULL);
	*sec = (ulint) tv.tv_sec;
	*ms  = (ulint) tv.tv_usec;
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
	time_t	   tm;

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
	time_t	   tm;

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
	time_t	   tm;

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
	time_t	   tm;

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
	const void*	buf,	/* in: memory buffer */
	ulint		len)	/* in: length of the buffer */
{
	const byte*	data;
	ulint		i;

	UNIV_MEM_ASSERT_RW(buf, len);

	fprintf(file, " len %lu; hex ", len);

	for (data = (const byte*)buf, i = 0; i < len; i++) {
		fprintf(file, "%02lx", (ulong)*data++);
	}

	fputs("; asc ", file);

	data = (const byte*)buf;

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
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */

void
ut_print_name(
/*==========*/
	FILE*		f,	/* in: output stream */
	trx_t*		trx,	/* in: transaction */
	ibool		table_id,/* in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name)	/* in: name to print */
{
	ut_print_namel(f, trx, table_id, name, strlen(name));
}

/**************************************************************************
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */

void
ut_print_namel(
/*===========*/
	FILE*		f,	/* in: output stream */
	trx_t*		trx,	/* in: transaction (NULL=no quotes) */
	ibool		table_id,/* in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name,	/* in: name to print */
	ulint		namelen)/* in: length of name */
{
#ifdef UNIV_HOTBACKUP
	fwrite(name, 1, namelen, f);
#else
	if (table_id) {
		char*	slash = memchr(name, '/', namelen);
		if (!slash) {

			goto no_db_name;
		}

		/* Print the database name and table name separately. */
		innobase_print_identifier(f, trx, TRUE, name, slash - name);
		putc('.', f);
		innobase_print_identifier(f, trx, TRUE, slash + 1,
					  namelen - (slash - name) - 1);
	} else {
no_db_name:
		innobase_print_identifier(f, trx, table_id, name, namelen);
	}
#endif
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
		size_t	maxs = len < (long) sizeof buf
			? (size_t) len
			: sizeof buf;
		size_t	size = fread(buf, 1, maxs, src);
		fwrite(buf, 1, size, dest);
		len -= (long) size;
		if (size < maxs) {
			break;
		}
	} while (len > 0);
}

/**************************************************************************
snprintf(). */

#ifdef __WIN__
#include <stdarg.h>
int
ut_snprintf(
				/* out: number of characters that would
				have been printed if the size were
				unlimited, not including the terminating
				'\0'. */
	char*		str,	/* out: string */
	size_t		size,	/* in: str size */
	const char*	fmt,	/* in: format */
	...)			/* in: format values */
{
	int	res;
	va_list	ap1;
	va_list	ap2;

	va_start(ap1, fmt);
	va_start(ap2, fmt);

	res = _vscprintf(fmt, ap1);
	ut_a(res != -1);

	if (size > 0) {
		_vsnprintf(str, size, fmt, ap2);

		if ((size_t) res >= size) {
			str[size - 1] = '\0';
		}
	}

	va_end(ap1);
	va_end(ap2);

	return(res);
}
#endif /* __WIN__ */
