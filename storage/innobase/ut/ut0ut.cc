/*****************************************************************************

Copyright (c) 1994, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef UNIV_INNOCHECKSUM

#ifndef UNIV_HOTBACKUP
# include <mysql_com.h>
#endif /* !UNIV_HOTBACKUP */

#include "os0thread.h"
#include "ut0ut.h"

#ifdef UNIV_NONINL
#include "ut0ut.ic"
#endif

#ifndef UNIV_HOTBACKUP
# include "trx0trx.h"
#endif /* !UNIV_HOTBACKUP */

#include "log.h"

#ifdef _WIN32
#include <mysql/innodb_priv.h> /* For sql_print_error */
typedef VOID(WINAPI *time_fn)(LPFILETIME);
static time_fn ut_get_system_time_as_file_time = GetSystemTimeAsFileTime;

/*****************************************************************//**
NOTE: The Windows epoch starts from 1601/01/01 whereas the Unix
epoch starts from 1970/1/1. For selection of constant see:
http://support.microsoft.com/kb/167296/ */
#define WIN_TO_UNIX_DELTA_USEC	11644473600000000LL


/**
Initialise highest available time resolution API on Windows
@return 0 if all OK else -1 */
int
ut_win_init_time()
{
	HMODULE h = LoadLibrary("kernel32.dll");
	if (h != NULL)
	{
		time_fn pfn = (time_fn)GetProcAddress(h, "GetSystemTimePreciseAsFileTime");
		if (pfn != NULL)
		{
			ut_get_system_time_as_file_time = pfn;
		}
		return false;
	}
	DWORD error = GetLastError();
  sql_print_error(
		"LoadLibrary(\"kernel32.dll\") failed: GetLastError returns %lu", error);
	return(-1);
}

/*****************************************************************//**
This is the Windows version of gettimeofday(2).
@return 0 if all OK else -1 */
static
int
ut_gettimeofday(
/*============*/
	struct timeval*	tv,	/*!< out: Values are relative to Unix epoch */
	void*		tz)	/*!< in: not used */
{
	FILETIME	ft;
	int64_t		tm;

	if (!tv) {
		errno = EINVAL;
		return(-1);
	}

	ut_get_system_time_as_file_time(&ft);

	tm = (int64_t) ft.dwHighDateTime << 32;
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
@return system time */
ib_time_t
ut_time(void)
/*=========*/
{
	return(time(NULL));
}

#ifndef UNIV_HOTBACKUP

/** Returns the number of microseconds since epoch. Uses the monotonic clock.
 For windows it return normal time.
 @return us since epoch or 0 if failed to retrieve */
ib_time_monotonic_us_t ut_time_monotonic_us(void) {
	uintmax_t	us;
#ifdef HAVE_CLOCK_GETTIME
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC,&tp);
	us = static_cast<uintmax_t>(tp.tv_sec) *1000000 + tp.tv_nsec / 1000;
#else
	struct timeval	tv;
	ut_gettimeofday(&tv, NULL);
	us = static_cast<uintmax_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif /* HAVE_CLOCK_GETTIME */
	return(us);
}

/** Returns the number of milliseconds since epoch. Uses the monotonic clock.
 For windows,MacOS it return normal time.
 @return us since epoch or 0 if failed to retrieve */
ib_time_monotonic_ms_t ut_time_monotonic_ms(void) {
#ifdef HAVE_CLOCK_GETTIME
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC,&tp);
	return ((ulint) tp.tv_sec * 1000 + tp.tv_nsec / 1000 / 1000);
#else
	struct timeval	tv;
	ut_gettimeofday(&tv, NULL);
	return((ulint) tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif /* HAVE_CLOCK_GETTIME */
}

/** Returns the number of seconds since epoch. Uses the monotonic clock.
 For windows it return normal time.
 @return us since epoch or 0 if failed to retrieve */
ib_time_monotonic_us_t ut_time_monotonic(void) {
#ifdef HAVE_CLOCK_GETTIME
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC,&tp);
	return tp.tv_sec;
#else
	return(time(NULL));
#endif /* HAVE_CLOCK_GETTIME */

}

#endif /* !UNIV_HOTBACKUP */

/**********************************************************//**
Returns the difference of two times in seconds.
@return time2 - time1 expressed in seconds */
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
void
ut_print_timestamp(
/*===============*/
	FILE*  file) /*!< in: file where to print */
{
	ulint thread_id = 0;

#ifndef UNIV_INNOCHECKSUM
	thread_id = os_thread_pf(os_thread_get_curr_id());
#endif /* !UNIV_INNOCHECKSUM */

#ifdef _WIN32
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	fprintf(file, "%d-%02d-%02d %02d:%02d:%02d %#llx",
		(int) cal_tm.wYear,
		(int) cal_tm.wMonth,
		(int) cal_tm.wDay,
		(int) cal_tm.wHour,
		(int) cal_tm.wMinute,
		(int) cal_tm.wSecond,
		static_cast<ulonglong>(thread_id));
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
	fprintf(file, "%d-%02d-%02d %02d:%02d:%02d %#lx",
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
void
ut_sprintf_timestamp(
/*=================*/
	char*	buf) /*!< in: buffer where to sprintf */
{
#ifdef _WIN32
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

	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
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
void
ut_sprintf_timestamp_without_extra_chars(
/*=====================================*/
	char*	buf) /*!< in: buffer where to sprintf */
{
#ifdef _WIN32
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

	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
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
void
ut_get_year_month_day(
/*==================*/
	ulint*	year,	/*!< out: current year */
	ulint*	month,	/*!< out: month */
	ulint*	day)	/*!< out: day */
{
#ifdef _WIN32
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	*year = (ulint) cal_tm.wYear;
	*month = (ulint) cal_tm.wMonth;
	*day = (ulint) cal_tm.wDay;
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
	*year = (ulint) cal_tm_ptr->tm_year + 1900;
	*month = (ulint) cal_tm_ptr->tm_mon + 1;
	*day = (ulint) cal_tm_ptr->tm_mday;
#endif
}

#else /* UNIV_HOTBACKUP */

/*************************************************************//**
Runs an idle loop on CPU. The argument gives the desired delay
in microseconds on 100 MHz Pentium + Visual C++.
@return dummy value */
ulint
ut_delay(
/*=====*/
	ulint	delay)	/*!< in: delay in microseconds on 100 MHz Pentium */
{
	ulint	i, j;

	UT_LOW_PRIORITY_CPU();

	j = 0;

	for (i = 0; i < delay * 50; i++) {
		j += i;
		UT_RELAX_CPU();
	}

	UT_RESUME_PRIORITY_CPU();

	return(j);
}
#endif /* UNIV_HOTBACKUP */

/*************************************************************//**
Prints the contents of a memory buffer in hex and ascii. */
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

	fprintf(file, " len " ULINTPF "; hex ", len);

	for (data = (const byte*) buf, i = 0; i < len; i++) {
		fprintf(file, "%02lx", static_cast<ulong>(*data++));
	}

	fputs("; asc ", file);

	data = (const byte*) buf;

	for (i = 0; i < len; i++) {
		int	c = (int) *data++;
		putc(isprint(c) ? c : ' ', file);
	}

	putc(';', file);
}

/*************************************************************//**
Prints the contents of a memory buffer in hex. */
void
ut_print_buf_hex(
/*=============*/
	std::ostream&	o,	/*!< in/out: output stream */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len)	/*!< in: length of the buffer */
{
	const byte*		data;
	ulint			i;

	static const char	hexdigit[16] = {
		'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
	};

	UNIV_MEM_ASSERT_RW(buf, len);

	o << "(0x";

	for (data = static_cast<const byte*>(buf), i = 0; i < len; i++) {
		byte	b = *data++;
		o << hexdigit[(int) b >> 16] << hexdigit[b & 15];
	}

	o << ")";
}

/*************************************************************//**
Prints the contents of a memory buffer in hex and ascii. */
void
ut_print_buf(
/*=========*/
	std::ostream&	o,	/*!< in/out: output stream */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len)	/*!< in: length of the buffer */
{
	const byte*	data;
	ulint		i;

	UNIV_MEM_ASSERT_RW(buf, len);

	for (data = static_cast<const byte*>(buf), i = 0; i < len; i++) {
		int	c = static_cast<int>(*data++);
		o << (isprint(c) ? static_cast<char>(c) : ' ');
	}

	ut_print_buf_hex(o, buf, len);
}

/*************************************************************//**
Calculates fast the number rounded up to the nearest power of 2.
@return first power of 2 which is >= n */
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

#ifndef UNIV_HOTBACKUP
/** Get a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier.
 @param		[in]	trx		transaction (NULL=no quotes).
 @param		[in]	name		table name.
 @retval	String quoted as an SQL identifier.
*/
std::string
ut_get_name(
	const trx_t*	trx,
	const char*	name)
{
	/* 2 * NAME_LEN for database and table name,
	and some slack for the #mysql50# prefix and quotes */
	char		buf[3 * NAME_LEN];
	const char*	bufend;

	bufend = innobase_convert_name(buf, sizeof buf,
				       name, strlen(name),
				       trx ? trx->mysql_thd : NULL);
	buf[bufend - buf] = '\0';
	return(std::string(buf, 0, bufend - buf));
}

/**********************************************************************//**
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */
void
ut_print_name(
/*==========*/
	FILE*		f,	/*!< in: output stream */
	const trx_t*	trx,	/*!< in: transaction */
	const char*	name)	/*!< in: name to print */
{
	/* 2 * NAME_LEN for database and table name,
	and some slack for the #mysql50# prefix and quotes */
	char		buf[3 * NAME_LEN];
	const char*	bufend;

	bufend = innobase_convert_name(buf, sizeof buf,
				       name, strlen(name),
				       trx ? trx->mysql_thd : NULL);

	if (fwrite(buf, 1, bufend - buf, f) != (size_t) (bufend - buf)) {
		perror("fwrite");
	}
}

/** Format a table name, quoted as an SQL identifier.
If the name contains a slash '/', the result will contain two
identifiers separated by a period (.), as in SQL
database_name.table_name.
@see table_name_t
@param[in]	name		table or index name
@param[out]	formatted	formatted result, will be NUL-terminated
@param[in]	formatted_size	size of the buffer in bytes
@return pointer to 'formatted' */
char*
ut_format_name(
	const char*	name,
	char*		formatted,
	ulint		formatted_size)
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
				    name, strlen(name), NULL);

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
		if (fwrite(buf, 1, size, dest) != size) {
			perror("fwrite");
		}
		len -= (long) size;
		if (size < maxs) {
			break;
		}
	} while (len > 0);
}
#endif /* !UNIV_HOTBACKUP */

#ifdef _WIN32
# include <stdarg.h>
/**********************************************************************//**
A substitute for vsnprintf(3), formatted output conversion into
a limited buffer. Note: this function DOES NOT return the number of
characters that would have been printed if the buffer was unlimited because
VC's _vsnprintf() returns -1 in this case and we would need to call
_vscprintf() in addition to estimate that but we would need another copy
of "ap" for that and VC does not provide va_copy(). */
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
#endif /* _WIN32 */

/** Convert an error number to a human readable text message.
The returned string is static and should not be freed or modified.
@param[in]	num	InnoDB internal error number
@return string, describing the error */
const char*
ut_strerr(
	dberr_t	num)
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
	case DB_TABLESPACE_TRUNCATED:
		return("Tablespace was truncated");
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
	case DB_IDENTIFIER_TOO_LONG:
		return("Identifier name is too long");
	case DB_FTS_EXCEED_RESULT_CACHE_LIMIT:
		return("FTS query exceeds result cache limit");
	case DB_TEMP_FILE_WRITE_FAIL:
		return("Temp file write failure");
	case DB_CANT_CREATE_GEOMETRY_OBJECT:
		return("Can't create specificed geometry data object");
	case DB_CANNOT_OPEN_FILE:
		return("Cannot open a file");
	case DB_TABLE_CORRUPT:
		return("Table is corrupted");
	case DB_FTS_TOO_MANY_WORDS_IN_PHRASE:
		return("Too many words in a FTS phrase or proximity search");
	case DB_IO_DECOMPRESS_FAIL:
		return("Page decompress failed after reading from disk");
	case DB_IO_NO_PUNCH_HOLE:
		return("No punch hole support");
	case DB_IO_NO_PUNCH_HOLE_FS:
		return("Punch hole not supported by the file system");
	case DB_IO_NO_PUNCH_HOLE_TABLESPACE:
		return("Punch hole not supported by the tablespace");
	case DB_IO_NO_ENCRYPT_TABLESPACE:
		return("Page encryption not supported by the tablespace");
	case DB_IO_DECRYPT_FAIL:
		return("Page decryption failed after reading from disk");
	case DB_IO_PARTIAL_FAILED:
		return("Partial IO failed");
	case DB_FORCED_ABORT:
		return("Transaction aborted by another higher priority "
		       "transaction");
	case DB_WRONG_FILE_NAME:
		return("Invalid Filename");
	case DB_NO_FK_ON_S_BASE_COL:
		return("Cannot add foreign key on the base column "
		       "of stored column");
	case DB_COMPUTE_VALUE_FAILED:
		return("Compute generated column failed");

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

#ifdef UNIV_PFS_MEMORY

/** Extract the basename of a file without its extension.
For example, extract "foo0bar" out of "/path/to/foo0bar.cc".
@param[in]	file		file path, e.g. "/path/to/foo0bar.cc"
@param[out]	base		result, e.g. "foo0bar"
@param[in]	base_size	size of the output buffer 'base', if there
is not enough space, then the result will be truncated, but always
'\0'-terminated
@return number of characters that would have been printed if the size
were unlimited (not including the final ‘\0’) */
size_t
ut_basename_noext(
	const char*	file,
	char*		base,
	size_t		base_size)
{
	/* Assuming 'file' contains something like the following,
	extract the file name without the extenstion out of it by
	setting 'beg' and 'len'.
	...mysql-trunk/storage/innobase/dict/dict0dict.cc:302
                                             ^-- beg, len=9
	*/

	const char*	beg = strrchr(file, OS_PATH_SEPARATOR);

	if (beg == NULL) {
		beg = file;
	} else {
		beg++;
	}

	size_t		len = strlen(beg);

	const char*	end = strrchr(beg, '.');

	if (end != NULL) {
		len = end - beg;
	}

	const size_t	copy_len = std::min(len, base_size - 1);

	memcpy(base, beg, copy_len);

	base[copy_len] = '\0';

	return(len);
}

#endif /* UNIV_PFS_MEMORY */

namespace ib {

info::~info()
{
	sql_print_information("InnoDB: %s", m_oss.str().c_str());
}

warn::~warn()
{
	sql_print_warning("InnoDB: %s", m_oss.str().c_str());
}

error::~error()
{
	sql_print_error("InnoDB: %s", m_oss.str().c_str());
}

fatal::~fatal()
{
	sql_print_error("[FATAL] InnoDB: %s", m_oss.str().c_str());
	ut_error;
}

error_or_warn::~error_or_warn()
{
	if (m_error) {
		sql_print_error("InnoDB: %s", m_oss.str().c_str());
	} else {
		sql_print_warning("InnoDB: %s", m_oss.str().c_str());
	}
}

fatal_or_error::~fatal_or_error()
{
	sql_print_error("InnoDB: %s", m_oss.str().c_str());
	ut_a(!m_fatal);
}

} // namespace ib

#endif /* !UNIV_INNOCHECKSUM */
