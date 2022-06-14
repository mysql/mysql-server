/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

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

/** @file ut/ut0ut.cc
 Various utilities for Innobase.

 Created 5/11/1994 Heikki Tuuri
 ********************************************************************/

#include "my_config.h"

#include <errno.h>
#include <time.h>
#include <string>

#include "ha_prototypes.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef UNIV_HOTBACKUP
#include <mysql_com.h>
#endif /* !UNIV_HOTBACKUP */

#include "my_compiler.h"
#include "mysql_com.h"
#include "os0thread.h"
#include "ut0ut.h"

#ifndef UNIV_HOTBACKUP
#include "sql/log.h"
#include "trx0trx.h"
#endif /* !UNIV_HOTBACKUP */

#include "clone0api.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/derror.h"

namespace ut {
ulong spin_wait_pause_multiplier = 50;
}

#ifdef UNIV_HOTBACKUP
/** Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'.
@param[in]      buf     buffer where to sprintf */
void meb_sprintf_timestamp_without_extra_chars(
    char *buf) /*!< in: buffer where to sprintf */
{
#ifdef _WIN32
  SYSTEMTIME cal_tm;

  GetLocalTime(&cal_tm);

  sprintf(buf, "%02d%02d%02d_%2d_%02d_%02d", (int)cal_tm.wYear % 100,
          (int)cal_tm.wMonth, (int)cal_tm.wDay, (int)cal_tm.wHour,
          (int)cal_tm.wMinute, (int)cal_tm.wSecond);
#else
  struct tm *cal_tm_ptr;
  time_t tm;

  struct tm cal_tm;
  time(&tm);
  localtime_r(&tm, &cal_tm);
  cal_tm_ptr = &cal_tm;
  sprintf(buf, "%02d%02d%02d_%2d_%02d_%02d", cal_tm_ptr->tm_year % 100,
          cal_tm_ptr->tm_mon + 1, cal_tm_ptr->tm_mday, cal_tm_ptr->tm_hour,
          cal_tm_ptr->tm_min, cal_tm_ptr->tm_sec);
#endif
}

#else  /* UNIV_HOTBACKUP */

ulint ut_delay(ulint delay) {
  ulint i, j;
  /* We don't expect overflow here, as ut::spin_wait_pause_multiplier is limited
  to 100, and values of delay are not larger than @@innodb_spin_wait_delay
  which is limited by 1 000. Anyway, in case an overflow happened, the program
  would still work (as iterations is unsigned). */
  const ulint iterations = delay * ut::spin_wait_pause_multiplier;
  UT_LOW_PRIORITY_CPU();

  j = 0;

  for (i = 0; i < iterations; i++) {
    j += i;
    UT_RELAX_CPU();
  }

  UT_RESUME_PRIORITY_CPU();

  return (j);
}
#endif /* UNIV_HOTBACKUP */

/** Calculates fast the number rounded up to the nearest power of 2.
 @return first power of 2 which is >= n */
ulint ut_2_power_up(ulint n) /*!< in: number != 0 */
{
  ulint res;

  res = 1;

  ut_ad(n > 0);

  while (res < n) {
    res = res * 2;
  }

  return (res);
}

#ifndef UNIV_HOTBACKUP
/** Get a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier.
 @param         [in]    trx             transaction (NULL=no quotes).
 @param         [in]    name            table name.
 @retval        String quoted as an SQL identifier.
*/
std::string ut_get_name(const trx_t *trx, const char *name) {
  /* 2 * NAME_LEN for database and table name,
  and some slack for the #mysql50# prefix and quotes */
  char buf[3 * NAME_LEN];
  const char *bufend;

  bufend = innobase_convert_name(buf, sizeof buf, name, strlen(name),
                                 trx ? trx->mysql_thd : nullptr);
  buf[bufend - buf] = '\0';
  return (std::string(buf, 0, bufend - buf));
}

/** Outputs a fixed-length string, quoted as an SQL identifier.
 If the string contains a slash '/', the string will be
 output as two identifiers separated by a period (.),
 as in SQL database_name.identifier. */
void ut_print_name(FILE *f,          /*!< in: output stream */
                   const trx_t *trx, /*!< in: transaction */
                   const char *name) /*!< in: name to print */
{
  /* 2 * NAME_LEN for database and table name,
  and some slack for the #mysql50# prefix and quotes */
  char buf[3 * NAME_LEN];
  const char *bufend;

  bufend = innobase_convert_name(buf, sizeof buf, name, strlen(name),
                                 trx ? trx->mysql_thd : nullptr);

  if (fwrite(buf, 1, bufend - buf, f) != (size_t)(bufend - buf)) {
    perror("fwrite");
  }
}

/** Format a table name, quoted as an SQL identifier.
If the name contains a slash '/', the result will contain two
identifiers separated by a period (.), as in SQL
database_name.table_name.
@see table_name_t
@param[in]      name            table or index name
@param[out]     formatted       formatted result, will be NUL-terminated
@param[in]      formatted_size  size of the buffer in bytes
@return pointer to 'formatted' */
char *ut_format_name(const char *name, char *formatted, ulint formatted_size) {
  switch (formatted_size) {
    case 1:
      formatted[0] = '\0';
      [[fallthrough]];
    case 0:
      return (formatted);
  }

  char *end;

  end = innobase_convert_name(formatted, formatted_size, name, strlen(name),
                              nullptr);

  /* If the space in 'formatted' was completely used, then sacrifice
  the last character in order to write '\0' at the end. */
  if ((ulint)(end - formatted) == formatted_size) {
    end--;
  }

  ut_a((ulint)(end - formatted) < formatted_size);

  *end = '\0';

  return (formatted);
}

/** Catenate files.
@param[in] dest Output file
@param[in] src Input file to be appended to output */
void ut_copy_file(FILE *dest, FILE *src) {
  long len = ftell(src);
  char buf[4096];

  rewind(src);
  do {
    size_t maxs = len < (long)sizeof buf ? (size_t)len : sizeof buf;
    size_t size = fread(buf, 1, maxs, src);
    if (fwrite(buf, 1, size, dest) != size) {
      perror("fwrite");
    }
    len -= (long)size;
    if (size < maxs) {
      break;
    }
  } while (len > 0);
}

void ut_format_byte_value(uint64_t data_bytes, std::string &data_str) {
  int unit_sz = 1024;
  auto exp = static_cast<int>(
      (data_bytes == 0) ? 0 : std::log(data_bytes) / std::log(unit_sz));
  auto data_value = data_bytes / std::pow(unit_sz, exp);

  char unit[] = " KMGTPE";
  auto index = static_cast<size_t>(exp > 0 ? exp : 0);

  /* 64 BIT number should never go beyond Exabyte. */
  auto max_index = sizeof(unit) - 2;
  if (index > max_index) {
    ut_d(ut_error);
    ut_o(index = max_index);
  }

  std::stringstream data_strm;
  if (index == 0) {
    data_strm << std::setprecision(2) << std::fixed << data_value << " "
              << "Bytes";
  } else {
    data_strm << std::setprecision(2) << std::fixed << data_value << " "
              << unit[index] << "iB";
  }
  data_str = data_strm.str();
}

#endif /* !UNIV_HOTBACKUP */

#ifdef _WIN32
#include <stdarg.h>

/** A substitute for vsnprintf(3), formatted output conversion into
 a limited buffer. Note: this function DOES NOT return the number of
 characters that would have been printed if the buffer was unlimited because
 VC's _vsnprintf() returns -1 in this case and we would need to call
 _vscprintf() in addition to estimate that but we would need another copy
 of "ap" for that and VC does not provide va_copy(). */
void ut_vsnprintf(char *str,       /*!< out: string */
                  size_t size,     /*!< in: str size */
                  const char *fmt, /*!< in: format */
                  va_list ap)      /*!< in: format values */
{
  _vsnprintf(str, size, fmt, ap);
  str[size - 1] = '\0';
}

#endif /* _WIN32 */

/** Convert an error number to a human readable text message.
The returned string is static and should not be freed or modified.
@param[in]      num     InnoDB internal error number
@return string, describing the error */
const char *ut_strerr(dberr_t num) {
  switch (num) {
    case DB_SUCCESS:
      return ("Success");
    case DB_SUCCESS_LOCKED_REC:
      return ("Success, record lock created");
    case DB_SKIP_LOCKED:
      return ("Skip locked records");
    case DB_LOCK_NOWAIT:
      return ("Don't wait for locks");
    case DB_ERROR:
      return ("Generic error");
    case DB_READ_ONLY:
      return ("Read only transaction");
    case DB_INTERRUPTED:
      return ("Operation interrupted");
    case DB_OUT_OF_MEMORY:
      return ("Cannot allocate memory");
    case DB_OUT_OF_FILE_SPACE:
    case DB_OUT_OF_DISK_SPACE:
      return ("Out of disk space");
    case DB_LOCK_WAIT:
      return ("Lock wait");
    case DB_DEADLOCK:
      return ("Deadlock");
    case DB_ROLLBACK:
      return ("Rollback");
    case DB_DUPLICATE_KEY:
      return ("Duplicate key");
    case DB_MISSING_HISTORY:
      return ("Required history data has been deleted");
    case DB_CLUSTER_NOT_FOUND:
      return ("Cluster not found");
    case DB_TABLE_NOT_FOUND:
      return ("Table not found");
    case DB_MUST_GET_MORE_FILE_SPACE:
      return ("More file space needed");
    case DB_TABLE_IS_BEING_USED:
      return ("Table is being used");
    case DB_TOO_BIG_RECORD:
      return ("Record too big");
    case DB_TOO_BIG_INDEX_COL:
      return ("Index columns size too big");
    case DB_LOCK_WAIT_TIMEOUT:
      return ("Lock wait timeout");
    case DB_NO_REFERENCED_ROW:
      return ("Referenced key value not found");
    case DB_ROW_IS_REFERENCED:
      return ("Row is referenced");
    case DB_CANNOT_ADD_CONSTRAINT:
      return ("Cannot add constraint");
    case DB_CORRUPTION:
      return ("Data structure corruption");
    case DB_CANNOT_DROP_CONSTRAINT:
      return ("Cannot drop constraint");
    case DB_NO_SAVEPOINT:
      return ("No such savepoint");
    case DB_TABLESPACE_EXISTS:
      return ("Tablespace already exists");
    case DB_TABLESPACE_DELETED:
      return ("Tablespace deleted or being deleted");
    case DB_TABLESPACE_NOT_FOUND:
      return ("Tablespace not found");
    case DB_LOCK_TABLE_FULL:
      return ("Lock structs have exhausted the buffer pool");
    case DB_FOREIGN_DUPLICATE_KEY:
      return ("Foreign key activated with duplicate keys");
    case DB_FOREIGN_EXCEED_MAX_CASCADE:
      return ("Foreign key cascade delete/update exceeds max depth");
    case DB_TOO_MANY_CONCURRENT_TRXS:
      return ("Too many concurrent transactions");
    case DB_UNSUPPORTED:
      return ("Unsupported");
    case DB_INVALID_NULL:
      return ("NULL value encountered in NOT NULL column");
    case DB_STATS_DO_NOT_EXIST:
      return ("Persistent statistics do not exist");
    case DB_FAIL:
      return ("Failed, retry may succeed");
    case DB_OVERFLOW:
      return ("Overflow");
    case DB_UNDERFLOW:
      return ("Underflow");
    case DB_STRONG_FAIL:
      return ("Failed, retry will not succeed");
    case DB_ZIP_OVERFLOW:
      return ("Zip overflow");
    case DB_RECORD_NOT_FOUND:
      return ("Record not found");
    case DB_CHILD_NO_INDEX:
      return ("No index on referencing keys in referencing table");
    case DB_PARENT_NO_INDEX:
      return ("No index on referenced keys in referenced table");
    case DB_FTS_INVALID_DOCID:
      return ("FTS Doc ID cannot be zero");
    case DB_INDEX_CORRUPT:
      return ("Index corrupted");
    case DB_UNDO_RECORD_TOO_BIG:
      return ("Undo record too big");
    case DB_END_OF_INDEX:
      return ("End of index");
    case DB_END_OF_BLOCK:
      return ("End of block");
    case DB_IO_ERROR:
      return ("I/O error");
    case DB_TABLE_IN_FK_CHECK:
      return ("Table is being used in foreign key check");
    case DB_DATA_MISMATCH:
      return ("data mismatch");
    case DB_SCHEMA_MISMATCH:
      return ("schema mismatch");
    case DB_NOT_FOUND:
      return ("not found");
    case DB_ONLINE_LOG_TOO_BIG:
      return ("Log size exceeded during online index creation");
    case DB_IDENTIFIER_TOO_LONG:
      return ("Identifier name is too long");
    case DB_FTS_EXCEED_RESULT_CACHE_LIMIT:
      return ("FTS query exceeds result cache limit");
    case DB_TEMP_FILE_WRITE_FAIL:
      return ("Temp file write failure");
    case DB_CANT_CREATE_GEOMETRY_OBJECT:
      return ("Can't create specificed geometry data object");
    case DB_CANNOT_OPEN_FILE:
      return ("Cannot open a file");
    case DB_TABLE_CORRUPT:
      return ("Table is corrupted");
    case DB_FTS_TOO_MANY_WORDS_IN_PHRASE:
      return ("Too many words in a FTS phrase or proximity search");
    case DB_IO_DECOMPRESS_FAIL:
      return ("Page decompress failed after reading from disk");
    case DB_IO_NO_PUNCH_HOLE:
      return ("No punch hole support");
    case DB_IO_NO_PUNCH_HOLE_FS:
      return ("Punch hole not supported by the file system");
    case DB_IO_NO_PUNCH_HOLE_TABLESPACE:
      return ("Punch hole not supported by the tablespace");
    case DB_IO_NO_ENCRYPT_TABLESPACE:
      return ("Page encryption not supported by the tablespace");
    case DB_IO_DECRYPT_FAIL:
      return ("Page decryption failed after reading from disk");
    case DB_IO_PARTIAL_FAILED:
      return ("Partial IO failed");
    case DB_FORCED_ABORT:
      return (
          "Transaction aborted by another higher priority "
          "transaction");
    case DB_WRONG_FILE_NAME:
      return ("Invalid Filename");
    case DB_NO_FK_ON_S_BASE_COL:
      return (
          "Cannot add foreign key on the base column "
          "of stored column");
    case DB_COMPUTE_VALUE_FAILED:
      return ("Compute generated column failed");
    case DB_V1_DBLWR_INIT_FAILED:
      return (
          "Failed to initialize the doublewrite extents "
          "in the system tablespace");
    case DB_V1_DBLWR_CREATE_FAILED:
      return (
          "Failed to create the doublewrite extents "
          "in the system tablespace");
    case DB_DBLWR_INIT_FAILED:
      return ("Failed to create a doublewrite instance");
    case DB_DBLWR_NOT_EXISTS:
      return (
          "Failed to find a doublewrite buffer "
          "in the system tablespace");
    case DB_INVALID_ENCRYPTION_META:
      return ("Invalid encryption meta-data information");
    case DB_ABORT_INCOMPLETE_CLONE:
      return ("Incomplete cloned data directory");
    case DB_SERVER_VERSION_LOW:
      return (
          "Cannot boot server with lower version than that built the "
          "tablespace");
    case DB_NO_SESSION_TEMP:
      return ("No session temporary tablespace allocated");
    case DB_TOO_LONG_PATH:
      return (
          "Cannot create tablespace since the filepath is too long for this "
          "OS");
    case DB_BTREE_LEVEL_LIMIT_EXCEEDED:
      return ("Btree level limit exceeded");
    case DB_END_SAMPLE_READ:
      return ("Sample reader has been requested to stop sampling");
    case DB_OUT_OF_RESOURCES:
      return ("System has run out of resources");
    case DB_FTS_TOO_MANY_NESTED_EXP:
      return ("Too many nested sub-expressions in a full-text search");
    case DB_PAGE_IS_STALE:
      return "Page was discarded, was not written to storage.";
    case DB_AUTOINC_READ_ERROR:
      return "Auto-increment read failed";
    case DB_FILE_READ_BEYOND_SIZE:
      return "File read failure because of the read being beyond file size.";
    case DB_ERROR_UNSET:;
      /* Fall through. */

      /* do not add default: in order to produce a warning if new code
      is added to the enum but not added here */
  }

  /* we abort here because if unknown error code is given, this could
  mean that memory corruption has happened and someone's error-code
  variable has been overwritten with bogus data */
  ut_error;
}

namespace ib {

#if !defined(UNIV_HOTBACKUP) && !defined(UNIV_NO_ERR_MSGS)

void logger::log_event(std::string msg) {
  LogEvent()
      .type(LOG_TYPE_ERROR)
      .prio(m_level)
      .errcode(m_err)
      .subsys("InnoDB")
      .verbatim(msg.c_str());
}
logger::~logger() { log_event(m_oss.str()); }

/*
MSVS complains: Warning C4722: destructor never returns, potential memory leak.
But, the whole point of using ib::fatal temporary object is to cause an abort.
*/
MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4722)

fatal::~fatal() {
  log_event("[FATAL] " + m_oss.str());
  ut_dbg_assertion_failed("ib::fatal triggered", m_location.filename,
                          m_location.line);
}
// Restore the MSVS checks for Warning C4722, silenced for ib::fatal::~fatal().
MY_COMPILER_DIAGNOSTIC_POP()

fatal_or_error::~fatal_or_error() {
  if (m_fatal) {
    log_event("[FATAL] " + m_oss.str());
    ut_dbg_assertion_failed("ib::fatal_or_error triggered", m_location.filename,
                            m_location.line);
  }
}

#endif /* !UNIV_NO_ERR_MSGS */

}  // namespace ib
