/*****************************************************************************

Copyright (c) 1994, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/***************************************************************//**
@file ut/ut0ut.cc
Various utilities for Innobase.

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "ut0ut.h"

#ifndef UNIV_INNOCHECKSUM

#include "ut0sort.h"
#include "os0thread.h" /* thread-ID */

#ifdef UNIV_NONINL
#include "ut0ut.ic"
#endif

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifndef UNIV_HOTBACKUP
# include "trx0trx.h"
# include "ha_prototypes.h"
# include "mysql_com.h" /* NAME_LEN */
#endif /* UNIV_HOTBACKUP */

/** A constant to prevent the compiler from optimizing ut_delay() away. */
UNIV_INTERN ibool	ut_always_false	= FALSE;

#ifdef __WIN__
/*****************************************************************//**
NOTE: The Windows epoch starts from 1601/01/01 whereas the Unix
epoch starts from 1970/1/1. For selection of constant see:
http://support.microsoft.com/kb/167296/ */
#define WIN_TO_UNIX_DELTA_USEC  ((ib_int64_t) 11644473600000000ULL)


/*****************************************************************//**
This is the Windows version of gettimeofday(2).
@return	0 if all OK else -1 */
static
int
ut_gettimeofday(
/*============*/
	struct timeval*	tv,	/*!< out: Values are relative to Unix epoch */
	void*		tz)	/*!< in: not used */
{
	FILETIME	ft;
	ib_int64_t	tm;

	if (!tv) {
		errno = EINVAL;
		return(-1);
	}

	GetSystemTimeAsFileTime(&ft);

	tm = (ib_int64_t) ft.dwHighDateTime << 32;
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
/** An alias for gettimeofday(2).  On Microsoft Windows, we have to
reimplement this function. */
#define	ut_gettimeofday		gettimeofday
#endif

/**********************************************************//**
Returns system time. We do not specify the format of the time returned:
the only way to manipulate it is to use the function ut_difftime.
@return	system time */
UNIV_INTERN
ib_time_t
ut_time(void)
/*=========*/
{
	return(time(NULL));
}

#ifndef UNIV_HOTBACKUP
/**********************************************************//**
Returns system time.
Upon successful completion, the value 0 is returned; otherwise the
value -1 is returned and the global variable errno is set to indicate the
error.
@return	0 on success, -1 otherwise */
UNIV_INTERN
int
ut_usectime(
/*========*/
	ulint*	sec,	/*!< out: seconds since the Epoch */
	ulint*	ms)	/*!< out: microseconds since the Epoch+*sec */
{
	struct timeval	tv;
	int		ret;
	int		errno_gettimeofday;
	int		i;

	for (i = 0; i < 10; i++) {

		ret = ut_gettimeofday(&tv, NULL);

		if (ret == -1) {
			errno_gettimeofday = errno;
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: gettimeofday(): %s\n",
				strerror(errno_gettimeofday));
			os_thread_sleep(100000);  /* 0.1 sec */
			errno = errno_gettimeofday;
		} else {
			break;
		}
	}

	if (ret != -1) {
		*sec = (ulint) tv.tv_sec;
		*ms  = (ulint) tv.tv_usec;
	}

	return(ret);
}

/**********************************************************//**
Returns the number of microseconds since epoch. Similar to
time(3), the return value is also stored in *tloc, provided
that tloc is non-NULL.
@return	us since epoch */
UNIV_INTERN
ullint
ut_time_us(
/*=======*/
	ullint*	tloc)	/*!< out: us since epoch, if non-NULL */
{
	struct timeval	tv;
	ullint		us;

	ut_gettimeofday(&tv, NULL);

	us = (ullint) tv.tv_sec * 1000000 + tv.tv_usec;

	if (tloc != NULL) {
		*tloc = us;
	}

	return(us);
}

/**********************************************************//**
Returns the number of milliseconds since some epoch.  The
value may wrap around.  It should only be used for heuristic
purposes.
@return	ms since epoch */
UNIV_INTERN
ulint
ut_time_ms(void)
/*============*/
{
	struct timeval	tv;

	ut_gettimeofday(&tv, NULL);

	return((ulint) tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************//**
Returns the difference of two times in seconds.
@return	time2 - time1 expressed in seconds */
UNIV_INTERN
double
ut_difftime(
/*========*/
	ib_time_t	time2,	/*!< in: time */
	ib_time_t	time1)	/*!< in: time */
{
	return(difftime(time2, time1));
}

#endif /* !UNIV_INNOCHECKSUM */

/**********************************************************//**
Prints a timestamp to a file. */
UNIV_INTERN
void
ut_print_timestamp(
/*===============*/
	FILE*  file) /*!< in: file where to print */
{
	ulint thread_id = 0;

#ifndef UNIV_INNOCHECKSUM
	thread_id = os_thread_pf(os_thread_get_curr_id());
#endif

#ifdef __WIN__
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	fprintf(file, "%d-%02d-%02d %02d:%02d:%02d %lx",
		(int) cal_tm.wYear,
		(int) cal_tm.wMonth,
		(int) cal_tm.wDay,
		(int) cal_tm.wHour,
		(int) cal_tm.wMinute,
		(int) cal_tm.wSecond,
		thread_id);
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

#ifdef HAVE_LOCALTIME_R
	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
#else
	time(&tm);
	cal_tm_ptr = localtime(&tm);
#endif
	fprintf(file, "%d-%02d-%02d %02d:%02d:%02d %lx",
		cal_tm_ptr->tm_year + 1900,
		cal_tm_ptr->tm_mon + 1,
		cal_tm_ptr->tm_mday,
		cal_tm_ptr->tm_hour,
		cal_tm_ptr->tm_min,
		cal_tm_ptr->tm_sec,
		thread_id);
#endif
}

#ifndef UNIV_INNOCHECKSUM

/**********************************************************//**
Sprintfs a timestamp to a buffer, 13..14 chars plus terminating NUL. */
UNIV_INTERN
void
ut_sprintf_timestamp(
/*=================*/
	char*	buf) /*!< in: buffer where to sprintf */
{
#ifdef __WIN__
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	sprintf(buf, "%02d%02d%02d %2d:%02d:%02d",
		(int) cal_tm.wYear % 100,
		(int) cal_tm.wMonth,
		(int) cal_tm.wDay,
		(int) cal_tm.wHour,
		(int) cal_tm.wMinute,
		(int) cal_tm.wSecond);
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

#ifdef HAVE_LOCALTIME_R
	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
#else
	time(&tm);
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

#ifdef UNIV_HOTBACKUP
/**********************************************************//**
Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'. */
UNIV_INTERN
void
ut_sprintf_timestamp_without_extra_chars(
/*=====================================*/
	char*	buf) /*!< in: buffer where to sprintf */
{
#ifdef __WIN__
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	sprintf(buf, "%02d%02d%02d_%2d_%02d_%02d",
		(int) cal_tm.wYear % 100,
		(int) cal_tm.wMonth,
		(int) cal_tm.wDay,
		(int) cal_tm.wHour,
		(int) cal_tm.wMinute,
		(int) cal_tm.wSecond);
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

#ifdef HAVE_LOCALTIME_R
	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
#else
	time(&tm);
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

/**********************************************************//**
Returns current year, month, day. */
UNIV_INTERN
void
ut_get_year_month_day(
/*==================*/
	ulint*	year,	/*!< out: current year */
	ulint*	month,	/*!< out: month */
	ulint*	day)	/*!< out: day */
{
#ifdef __WIN__
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	*year = (ulint) cal_tm.wYear;
	*month = (ulint) cal_tm.wMonth;
	*day = (ulint) cal_tm.wDay;
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

#ifdef HAVE_LOCALTIME_R
	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
#else
	time(&tm);
	cal_tm_ptr = localtime(&tm);
#endif
	*year = (ulint) cal_tm_ptr->tm_year + 1900;
	*month = (ulint) cal_tm_ptr->tm_mon + 1;
	*day = (ulint) cal_tm_ptr->tm_mday;
#endif
}
#endif /* UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Runs an idle loop on CPU. The argument gives the desired delay
in microseconds on 100 MHz Pentium + Visual C++.
@return	dummy value */
UNIV_INTERN
ulint
ut_delay(
/*=====*/
	ulint	delay)	/*!< in: delay in microseconds on 100 MHz Pentium */
{
	ulint	i, j;

	j = 0;

	for (i = 0; i < delay * 50; i++) {
		j += i;
		UT_RELAX_CPU();
	}

	if (ut_always_false) {
		ut_always_false = (ibool) j;
	}

	return(j);
}
#endif /* !UNIV_HOTBACKUP */

/*************************************************************//**
Prints the contents of a memory buffer in hex and ascii. */
UNIV_INTERN
void
ut_print_buf(
/*=========*/
	FILE*		file,	/*!< in: file where to print */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len)	/*!< in: length of the buffer */
{
	const byte*	data;
	ulint		i;

	UNIV_MEM_ASSERT_RW(buf, len);

	fprintf(file, " len %lu; hex ", len);

	for (data = (const byte*) buf, i = 0; i < len; i++) {
		fprintf(file, "%02lx", (ulong)*data++);
	}

	fputs("; asc ", file);

	data = (const byte*) buf;

	for (i = 0; i < len; i++) {
		int	c = (int) *data++;
		putc(isprint(c) ? c : ' ', file);
	}

	putc(';', file);
}

/**********************************************************************//**
Sort function for ulint arrays. */
UNIV_INTERN
void
ut_ulint_sort(
/*==========*/
	ulint*	arr,		/*!< in/out: array to sort */
	ulint*	aux_arr,	/*!< in/out: aux array to use in sort */
	ulint	low,		/*!< in: lower bound */
	ulint	high)		/*!< in: upper bound */
{
	UT_SORT_FUNCTION_BODY(ut_ulint_sort, arr, aux_arr, low, high,
			      ut_ulint_cmp);
}

/*************************************************************//**
Calculates fast the number rounded up to the nearest power of 2.
@return	first power of 2 which is >= n */
UNIV_INTERN
ulint
ut_2_power_up(
/*==========*/
	ulint	n)	/*!< in: number != 0 */
{
	ulint	res;

	res = 1;

	ut_ad(n > 0);

	while (res < n) {
		res = res * 2;
	}

	return(res);
}

/**********************************************************************//**
Outputs a NUL-terminated file name, quoted with apostrophes. */
UNIV_INTERN
void
ut_print_filename(
/*==============*/
	FILE*		f,	/*!< in: output stream */
	const char*	name)	/*!< in: name to print */
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
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */
UNIV_INTERN
void
ut_print_name(
/*==========*/
	FILE*		f,	/*!< in: output stream */
	const trx_t*	trx,	/*!< in: transaction */
	ibool		table_id,/*!< in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name)	/*!< in: name to print */
{
	ut_print_namel(f, trx, table_id, name, strlen(name));
}

/**********************************************************************//**
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */
UNIV_INTERN
void
ut_print_namel(
/*===========*/
	FILE*		f,	/*!< in: output stream */
	const trx_t*	trx,	/*!< in: transaction (NULL=no quotes) */
	ibool		table_id,/*!< in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name,	/*!< in: name to print */
	ulint		namelen)/*!< in: length of name */
{
	/* 2 * NAME_LEN for database and table name,
	and some slack for the #mysql50# prefix and quotes */
	char		buf[3 * NAME_LEN];
	const char*	bufend;

	bufend = innobase_convert_name(buf, sizeof buf,
				       name, namelen,
				       trx ? trx->mysql_thd : NULL,
				       table_id);

	fwrite(buf, 1, bufend - buf, f);
}

/**********************************************************************//**
Formats a table or index name, quoted as an SQL identifier. If the name
contains a slash '/', the result will contain two identifiers separated by
a period (.), as in SQL database_name.identifier.
@return pointer to 'formatted' */
UNIV_INTERN
char*
ut_format_name(
/*===========*/
	const char*	name,		/*!< in: table or index name, must be
					'\0'-terminated */
	ibool		is_table,	/*!< in: if TRUE then 'name' is a table
					name */
	char*		formatted,	/*!< out: formatted result, will be
					'\0'-terminated */
	ulint		formatted_size)	/*!< out: no more than this number of
					bytes will be written to 'formatted' */
{
	switch (formatted_size) {
	case 1:
		formatted[0] = '\0';
		/* FALL-THROUGH */
	case 0:
		return(formatted);
	}

	char*	end;

	end = innobase_convert_name(formatted, formatted_size,
				    name, strlen(name), NULL, is_table);

	/* If the space in 'formatted' was completely used, then sacrifice
	the last character in order to write '\0' at the end. */
	if ((ulint) (end - formatted) == formatted_size) {
		end--;
	}

	ut_a((ulint) (end - formatted) < formatted_size);

	*end = '\0';

	return(formatted);
}

/**********************************************************************//**
Catenate files. */
UNIV_INTERN
void
ut_copy_file(
/*=========*/
	FILE*	dest,	/*!< in: output file */
	FILE*	src)	/*!< in: input file to be appended to output */
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
#endif /* !UNIV_HOTBACKUP */

#ifdef __WIN__
# include <stdarg.h>
/**********************************************************************//**
A substitute for vsnprintf(3), formatted output conversion into
a limited buffer. Note: this function DOES NOT return the number of
characters that would have been printed if the buffer was unlimited because
VC's _vsnprintf() returns -1 in this case and we would need to call
_vscprintf() in addition to estimate that but we would need another copy
of "ap" for that and VC does not provide va_copy(). */
UNIV_INTERN
void
ut_vsnprintf(
/*=========*/
	char*		str,	/*!< out: string */
	size_t		size,	/*!< in: str size */
	const char*	fmt,	/*!< in: format */
	va_list		ap)	/*!< in: format values */
{
	_vsnprintf(str, size, fmt, ap);
	str[size - 1] = '\0';
}

/**********************************************************************//**
A substitute for snprintf(3), formatted output conversion into
a limited buffer.
@return number of characters that would have been printed if the size
were unlimited, not including the terminating '\0'. */
UNIV_INTERN
int
ut_snprintf(
/*========*/
	char*		str,	/*!< out: string */
	size_t		size,	/*!< in: str size */
	const char*	fmt,	/*!< in: format */
	...)			/*!< in: format values */
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

/*************************************************************//**
Convert an error number to a human readable text message. The
returned string is static and should not be freed or modified.
@return	string, describing the error */
UNIV_INTERN
const char*
ut_strerr(
/*======*/
	dberr_t	num)	/*!< in: error number */
{
	switch (num) {
	case DB_SUCCESS:
		return("Success");
	case DB_SUCCESS_LOCKED_REC:
		return("Success, record lock created");
	case DB_ERROR:
		return("Generic error");
	case DB_READ_ONLY:
		return("Read only transaction");
	case DB_INTERRUPTED:
		return("Operation interrupted");
	case DB_OUT_OF_MEMORY:
		return("Cannot allocate memory");
	case DB_OUT_OF_FILE_SPACE:
		return("Out of disk space");
	case DB_LOCK_WAIT:
		return("Lock wait");
	case DB_DEADLOCK:
		return("Deadlock");
	case DB_ROLLBACK:
		return("Rollback");
	case DB_DUPLICATE_KEY:
		return("Duplicate key");
	case DB_QUE_THR_SUSPENDED:
		return("The queue thread has been suspended");
	case DB_MISSING_HISTORY:
		return("Required history data has been deleted");
	case DB_CLUSTER_NOT_FOUND:
		return("Cluster not found");
	case DB_TABLE_NOT_FOUND:
		return("Table not found");
	case DB_MUST_GET_MORE_FILE_SPACE:
		return("More file space needed");
	case DB_TABLE_IS_BEING_USED:
		return("Table is being used");
	case DB_TOO_BIG_RECORD:
		return("Record too big");
	case DB_TOO_BIG_INDEX_COL:
		return("Index columns size too big");
	case DB_LOCK_WAIT_TIMEOUT:
		return("Lock wait timeout");
	case DB_NO_REFERENCED_ROW:
		return("Referenced key value not found");
	case DB_ROW_IS_REFERENCED:
		return("Row is referenced");
	case DB_CANNOT_ADD_CONSTRAINT:
		return("Cannot add constraint");
	case DB_CORRUPTION:
		return("Data structure corruption");
	case DB_CANNOT_DROP_CONSTRAINT:
		return("Cannot drop constraint");
	case DB_NO_SAVEPOINT:
		return("No such savepoint");
	case DB_TABLESPACE_EXISTS:
		return("Tablespace already exists");
	case DB_TABLESPACE_DELETED:
		return("Tablespace deleted or being deleted");
	case DB_TABLESPACE_NOT_FOUND:
		return("Tablespace not found");
	case DB_LOCK_TABLE_FULL:
		return("Lock structs have exhausted the buffer pool");
	case DB_FOREIGN_DUPLICATE_KEY:
		return("Foreign key activated with duplicate keys");
	case DB_FOREIGN_EXCEED_MAX_CASCADE:
		return("Foreign key cascade delete/update exceeds max depth");
	case DB_TOO_MANY_CONCURRENT_TRXS:
		return("Too many concurrent transactions");
	case DB_UNSUPPORTED:
		return("Unsupported");
	case DB_INVALID_NULL:
		return("NULL value encountered in NOT NULL column");
	case DB_STATS_DO_NOT_EXIST:
		return("Persistent statistics do not exist");
	case DB_FAIL:
		return("Failed, retry may succeed");
	case DB_OVERFLOW:
		return("Overflow");
	case DB_UNDERFLOW:
		return("Underflow");
	case DB_STRONG_FAIL:
		return("Failed, retry will not succeed");
	case DB_ZIP_OVERFLOW:
		return("Zip overflow");
	case DB_RECORD_NOT_FOUND:
		return("Record not found");
	case DB_CHILD_NO_INDEX:
		return("No index on referencing keys in referencing table");
	case DB_PARENT_NO_INDEX:
		return("No index on referenced keys in referenced table");
	case DB_FTS_INVALID_DOCID:
		return("FTS Doc ID cannot be zero");
	case DB_INDEX_CORRUPT:
		return("Index corrupted");
	case DB_UNDO_RECORD_TOO_BIG:
		return("Undo record too big");
	case DB_END_OF_INDEX:
		return("End of index");
	case DB_IO_ERROR:
		return("I/O error");
	case DB_TABLE_IN_FK_CHECK:
		return("Table is being used in foreign key check");
	case DB_DATA_MISMATCH:
		return("data mismatch");
	case DB_SCHEMA_NOT_LOCKED:
		return("schema not locked");
	case DB_NOT_FOUND:
		return("not found");
	case DB_ONLINE_LOG_TOO_BIG:
		return("Log size exceeded during online index creation");

	/* do not add default: in order to produce a warning if new code
	is added to the enum but not added here */
	}

	/* we abort here because if unknown error code is given, this could
	mean that memory corruption has happened and someone's error-code
	variable has been overwritten with bogus data */
	ut_error;

	/* NOT REACHED */
	return("Unknown error");
}
#endif /* !UNIV_INNOCHECKSUM */
