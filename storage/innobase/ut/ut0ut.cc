/*****************************************************************************

Copyright (c) 1994, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/***************************************************************//**
@file ut/ut0ut.cc
Various utilities for Innobase.

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "my_config.h"

#include <errno.h>
#include <time.h>

#include "ha_prototypes.h"

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef UNIV_HOTBACKUP
# include <mysql_com.h>
#endif /* !UNIV_HOTBACKUP */

#include "os0thread.h"
#include "ut0ut.h"

#ifndef UNIV_HOTBACKUP
# include "trx0trx.h"
# include "sql/log.h"
#endif /* !UNIV_HOTBACKUP */


#ifdef _WIN32
using time_fn = VOID(WINAPI *)(_Out_ LPFILETIME);
static time_fn ut_get_system_time_as_file_time = GetSystemTimeAsFileTime;

/*****************************************************************//**
NOTE: The Windows epoch starts from 1601/01/01 whereas the Unix
epoch starts from 1970/1/1. For selection of constant see:
http://support.microsoft.com/kb/167296/ */
#define WIN_TO_UNIX_DELTA_USEC	11644473600000000LL


/**
Initialise highest available time resolution API on Windows
@return false if all OK else true */
bool
ut_win_init_time()
{
	HMODULE h = LoadLibrary("kernel32.dll");
	if (h != nullptr)
	{
		auto	pfn = reinterpret_cast<time_fn>(
			GetProcAddress(h, "GetSystemTimePreciseAsFileTime"));
		if (pfn != nullptr)
		{
			ut_get_system_time_as_file_time = pfn;
		}
		return false;
	}
	DWORD error = GetLastError();
#ifndef UNIV_HOTBACKUP
	log_errlog(ERROR_LEVEL, ER_WIN_LOAD_LIBRARY_FAILED, "kernel32.dll",
		error);
#else /* !UNIV_HOTBACKUP */
	fprintf(stderr,
		"LoadLibrary(\"kernel32.dll\") failed:"
		" GetLastError returns %lu", error);
#endif /* !UNIV_HOTBACKUP */
	return(true);
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

/**********************************************************//**
Returns system time.
Upon successful completion, the value 0 is returned; otherwise the
value -1 is returned and the global variable errno is set to indicate the
error.
@return 0 on success, -1 otherwise */
int
ut_usectime(
/*========*/
	ulint*	sec,	/*!< out: seconds since the Epoch */
	ulint*	ms)	/*!< out: microseconds since the Epoch+*sec */
{
	struct timeval	tv;
	int		ret = 0;
	int		errno_gettimeofday;
	int		i;

	for (i = 0; i < 10; i++) {

		ret = ut_gettimeofday(&tv, NULL);

		if (ret == -1) {
			errno_gettimeofday = errno;
			ib::error() << "gettimeofday(): "
				<< strerror(errno_gettimeofday);
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
@return us since epoch */
uintmax_t
ut_time_us(
/*=======*/
	uintmax_t*	tloc)	/*!< out: us since epoch, if non-NULL */
{
	struct timeval	tv;
	uintmax_t	us;

	ut_gettimeofday(&tv, NULL);

	us = static_cast<uintmax_t>(tv.tv_sec) * 1000000 + tv.tv_usec;

	if (tloc != NULL) {
		*tloc = us;
	}

	return(us);
}

/**********************************************************//**
Returns the number of milliseconds since some epoch.  The
value may wrap around.  It should only be used for heuristic
purposes.
@return ms since epoch */
ulint
ut_time_ms(void)
/*============*/
{
	struct timeval	tv;

	ut_gettimeofday(&tv, NULL);

	return((ulint) tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

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

#ifdef UNIV_HOTBACKUP
/** Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'.
@param[in]	buf	buffer where to sprintf */
void
meb_sprintf_timestamp_without_extra_chars(
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
	case DB_SKIP_LOCKED:
		return("Skip locked records");
	case DB_LOCK_NOWAIT:
		return("Don't wait for locks");
	case DB_ERROR:
		return("Generic error");
	case DB_READ_ONLY:
		return("Read only transaction");
	case DB_INTERRUPTED:
		return("Operation interrupted");
	case DB_OUT_OF_MEMORY:
		return("Cannot allocate memory");
	case DB_OUT_OF_FILE_SPACE:
	case DB_OUT_OF_DISK_SPACE:
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

#ifndef UNIV_HOTBACKUP
namespace ib {

info::~info()
{
	log_errlog(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
		m_oss.str().c_str());
}

warn::~warn()
{
	log_errlog(WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG, m_oss.str().c_str());
}

error::~error()
{
	log_errlog(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG, m_oss.str().c_str());
}

fatal::~fatal()
{
	log_errlog(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_FATAL_MSG,
		m_oss.str().c_str());
	ut_error;
}

error_or_warn::~error_or_warn()
{
	if (m_error) {
		log_errlog(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
			m_oss.str().c_str());
	} else {
		log_errlog(WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
			m_oss.str().c_str());
	}
}

fatal_or_error::~fatal_or_error()
{
	log_errlog(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG, m_oss.str().c_str());
	ut_a(!m_fatal);
}

} // namespace ib

#endif /* !UNIV_HOTBACKUP */
