/*****************************************************************************

Copyright (c) 2006, 2022, Oracle and/or its affiliates.

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

/** @file include/ha_prototypes.h
 Prototypes for global functions in ha_innodb.cc that are called by
 InnoDB C code.

 NOTE: This header is intended to insulate InnoDB from SQL names and functions.
 Do not include any headers other than univ.i into this unless they are very
 simple headers.
 ************************************************************************/

#ifndef HA_INNODB_PROTOTYPES_H
#define HA_INNODB_PROTOTYPES_H

#include "univ.i"

/* Forward declarations */
class ha_innobase;
class THD;
class MDL_ticket;
struct CHARSET_INFO;
struct dict_table_t;

/** Formats the raw data in "data" (in InnoDB on-disk format) that is of
 type DATA_(CHAR|VARCHAR|MYSQL|VARMYSQL) using "charset_coll" and writes
 the result to "buf". The result is converted to "system_charset_info".
 Not more than "buf_size" bytes are written to "buf".
 The result is always NUL-terminated (provided buf_size > 0) and the
 number of bytes that were written to "buf" is returned (including the
 terminating NUL).
 @return number of bytes that were written */
ulint innobase_raw_format(const char *data,   /*!< in: raw data */
                          ulint data_len,     /*!< in: raw data length
                                              in bytes */
                          ulint charset_coll, /*!< in: charset collation */
                          char *buf,          /*!< out: output buffer */
                          ulint buf_size);    /*!< in: output buffer size
                                              in bytes */

/** Quote a standard SQL identifier like tablespace, index or column name.
@param[in]      file    output stream
@param[in]      trx     InnoDB transaction, or NULL
@param[in]      id      identifier to quote */
void innobase_quote_identifier(FILE *file, trx_t *trx, const char *id);

/** Quote an standard SQL identifier like tablespace, index or column name.
Return the string as an std:string object.
@param[in]      trx     InnoDB transaction, or NULL
@param[in]      id      identifier to quote
@return a std::string with id properly quoted. */
std::string innobase_quote_identifier(trx_t *trx, const char *id);

/** Convert a table name to the MySQL system_charset_info (UTF-8).
 @return pointer to the end of buf */
char *innobase_convert_name(
    char *buf,      /*!< out: buffer for converted identifier */
    ulint buflen,   /*!< in: length of buf, in bytes */
    const char *id, /*!< in: table name to convert */
    ulint idlen,    /*!< in: length of id, in bytes */
    THD *thd);      /*!< in: MySQL connection thread, or NULL */

/** Returns true if the thread is the replication thread on the slave
 server. Used in srv_conc_enter_innodb() to determine if the thread
 should be allowed to enter InnoDB - the replication thread is treated
 differently than other threads. Also used in
 srv_conc_force_exit_innodb().
 @return true if thd is the replication thread */
bool thd_is_replication_slave_thread(THD *thd); /*!< in: thread handle */

/** Returns true if the transaction this thread is processing has edited
 non-transactional tables. Used by the deadlock detector when deciding
 which transaction to rollback in case of a deadlock - we try to avoid
 rolling back transactions that have edited non-transactional tables.
 @return true if non-transactional tables have been edited */
bool thd_has_edited_nontrans_tables(THD *thd); /*!< in: thread handle */

/** Prints info of a THD object (== user session thread) to the given file.
@param[in] f Output stream
@param[in] thd Pointer to a mysql thd object
@param[in] max_query_len Max query length to print, or 0 to use the default max
length */
void innobase_mysql_print_thd(FILE *f, THD *thd, uint max_query_len);

/** Converts a MySQL type to an InnoDB type. Note that this function returns
the 'mtype' of InnoDB. InnoDB differentiates between MySQL's old <= 4.1
VARCHAR and the new true VARCHAR in >= 5.0.3 by the 'prtype'.
@param[out]     unsigned_flag           DATA_UNSIGNED if an 'unsigned type';
at least ENUM and SET, and unsigned integer types are 'unsigned types'
@param[in]      f                       MySQL Field
@return DATA_BINARY, DATA_VARCHAR, ... */
ulint get_innobase_type_from_mysql_type(ulint *unsigned_flag, const void *f);

#include <sql/dd/types/column.h>
/** Converts a MySQL data-dictionary type to an InnoDB type. Also returns
a few attributes which are useful for precise type calculation.

@note This function is version of get_innobase_type_from_mysql_type() with
added knowledge about how additional attributes calculated (e.g. in
create_table_info_t::create_table_def()) and about behavior of Field
class and its descendats.

@note It allows to get InnoDB generic and precise types directly from MySQL
data-dictionary info, bypassing expensive construction of Field objects.

@param[out] unsigned_flag   DATA_UNSIGNED if an 'unsigned type'.
@param[out] binary_type     DATA_BINARY_TYPE if a 'binary type'.
@param[out] charset_no      Collation id for string types.
@param[in]  dd_type         MySQL data-dictionary type.
@param[in]  field_charset   Charset.
@param[in]  is_unsigned     MySQL data-dictionary unsigned flag.

@return DATA_BINARY, DATA_VARCHAR, ... */
ulint get_innobase_type_from_mysql_dd_type(ulint *unsigned_flag,
                                           ulint *binary_type,
                                           ulint *charset_no,
                                           dd::enum_column_types dd_type,
                                           const CHARSET_INFO *field_charset,
                                           bool is_unsigned);

/** Get the variable length bounds of the given character set.
@param[in] cset Mysql charset-collation code
@param[out] mbminlen Minimum length of a char (in bytes)
@param[out] mbmaxlen Maximum length of a char (in bytes) */
void innobase_get_cset_width(ulint cset, ulint *mbminlen, ulint *mbmaxlen);

/** Compares NUL-terminated UTF-8 strings case insensitively.
 @return 0 if a=b, < 0 if a < b, > 1 if a > b */
int innobase_strcasecmp(const char *a,  /*!< in: first string to compare */
                        const char *b); /*!< in: second string to compare */

/** Strip dir name from a full path name and return only the file name
@param[in]      path_name       full path name
@return file name or "null" if no file name */
const char *innobase_basename(const char *path_name);

/** Returns true if the thread is executing a SELECT statement.
 @return true if thd is executing SELECT */
bool thd_is_query_block(const THD *thd); /*!< in: thread handle */

/** Makes all characters in a NUL-terminated UTF-8 string lower case. */
void innobase_casedn_str(char *a); /*!< in/out: string to put in lower case */

/** Makes all characters in a NUL-terminated UTF-8 path string lower case. */
void innobase_casedn_path(char *a); /*!< in/out: string to put in lower case */

/** Determines the connection character set.
 @return connection character set */
const CHARSET_INFO *innobase_get_charset(
    THD *thd); /*!< in: MySQL thread handle */

/** Determines the current SQL statement.
Thread unsafe, can only be called from the thread owning the THD.
@param[in]      thd     MySQL thread handle
@param[out]     length  Length of the SQL statement
@return                 SQL statement string */
const char *innobase_get_stmt_unsafe(THD *thd, size_t *length);

/** Determines the current SQL statement.
Thread safe, can be called from any thread as the string is copied
into the provided buffer.
@param[in]      thd     MySQL thread handle
@param[out]     buf     Buffer containing SQL statement
@param[in]      buflen  Length of provided buffer
@return                 Length of the SQL statement */
size_t innobase_get_stmt_safe(THD *thd, char *buf, size_t buflen);

/** This function is used to find the storage length in bytes of the first n
 characters for prefix indexes using a multibyte character set. The function
 finds charset information and returns length of prefix_len characters in the
 index field in bytes.
 @return number of bytes occupied by the first n characters */
ulint innobase_get_at_most_n_mbchars(
    ulint charset_id, /*!< in: character set id */
    ulint prefix_len, /*!< in: prefix length in bytes of the index
                      (this has to be divided by mbmaxlen to get the
                      number of CHARACTERS n in the prefix) */
    ulint data_len,   /*!< in: length of the string in bytes */
    const char *str); /*!< in: character string */

/** Checks sys_vars and determines if allocator should mark
large memory segments with MADV_DONTDUMP
@return true iff @@global.core_file AND
NOT @@global.innodb_buffer_pool_in_core_file */
bool innobase_should_madvise_buf_pool();

/** Make sure that core file will not be generated, as generating a core file
might violate our promise to not dump buffer pool data, and/or might dump not
the expected memory pages due to failure in using madvise */
void innobase_disable_core_dump();

/** Returns the lock wait timeout for the current connection.
 @return the lock wait timeout */
std::chrono::seconds thd_lock_wait_timeout(
    THD *thd); /*!< in: thread handle, or NULL to query
the global innodb_lock_wait_timeout */

/** Set the time waited for the lock for the current query.
@param[in,out] thd Thread handle.
@param[in] value Time waited for the lock. */
void thd_set_lock_wait_time(THD *thd,
                            std::chrono::steady_clock::duration value);

/** Get the value of innodb_tmpdir.
@param[in] thd  thread handle, or nullptr to query the global innodb_tmpdir.
@return nullptr if innodb_tmpdir="" */
const char *thd_innodb_tmpdir(THD *thd);

#ifdef UNIV_DEBUG
/** Obtain the value of the latest output from InnoDB Interpreter/Tester
module (ib::Tester).
@param[in]      thd     thread handle
@return pointer to the output string. */
char **thd_innodb_interpreter_output(THD *thd);

/** Obtain the latest command executed by InnoDB Interpreter/Tester
module (ib::Tester).
@param[in]      thd     thread handle
@return pointer to the output string. */
char **thd_innodb_interpreter(THD *thd);
#endif /* UNIV_DEBUG */

/** Get the current setting of the table_cache_size global parameter. We do
 a dirty read because for one there is no synchronization object and
 secondly there is little harm in doing so even if we get a torn read.
 @return SQL statement string */
ulint innobase_get_table_cache_size(void);

/** Get the current setting of the lower_case_table_names global parameter from
 mysqld.cc. We do a dirty read because for one there is no synchronization
 object and secondly there is little harm in doing so even if we get a torn
 read.
 @return value of lower_case_table_names */
ulint innobase_get_lower_case_table_names(void);

/** Compare two character strings case insensitively according to their
charset.
@param[in]  cs  character set
@param[in]  s1  string 1
@param[in]  s2  string 2
@return 0 if the two strings are equal */
int innobase_nocase_compare(const void *cs, const char *s1, const char *s2);

/** Returns true if transaction should be flagged as read-only.
 @return true if the thd is marked as read-only */
bool thd_trx_is_read_only(THD *thd); /*!< in/out: thread handle */

/**
Check if the transaction can be rolled back
@param[in] requestor    Session requesting the lock
@param[in] holder       Session that holds the lock
@return the session that will be rolled back, null don't care */

THD *thd_trx_arbitrate(THD *requestor, THD *holder);

/**
@param[in] thd          Session to check
@return the priority */

int thd_trx_priority(THD *thd);

/** Check if the transaction is an auto-commit transaction. true also
 implies that it is a SELECT (read-only) transaction.
 @return true if the transaction is an auto commit read-only transaction. */
bool thd_trx_is_auto_commit(THD *thd); /*!< in: thread handle, or NULL */

/** Get the thread start time.
 @return the thread start time. */
std::chrono::system_clock::time_point thd_start_time(
    THD *thd); /*!< in: thread handle, or NULL */

/** A wrapper function of innobase_convert_name(), convert a table name
to the MySQL system_charset_info (UTF-8) and quote it if needed.
@param[out] buf Buffer for converted identifier
@param[in] buflen Length of buf, in bytes
@param[in] name Table name to format */
void innobase_format_name(char *buf, ulint buflen, const char *name);

/** Corresponds to Sql_condition:enum_warning_level. */
enum ib_log_level_t {
  IB_LOG_LEVEL_INFO,
  IB_LOG_LEVEL_WARN,
  IB_LOG_LEVEL_ERROR,
  IB_LOG_LEVEL_FATAL
};

/** Use this when the args are first converted to a formatted string and then
 passed to the format string from messages_to_clients.txt. The error message
 format must be: "Some string ... %s".

 Push a warning message to the client, it is a wrapper around:

 void push_warning_printf(
         THD *thd, Sql_condition::enum_warning_level level,
         uint code, const char *format, ...);
 */
void ib_errf(THD *thd,             /*!< in/out: session */
             ib_log_level_t level, /*!< in: warning level */
             uint32_t code,        /*!< MySQL error code */
             const char *format,   /*!< printf format */
             ...)                  /*!< Args */
    MY_ATTRIBUTE((format(printf, 4, 5)));

/** Use this when the args are passed to the format string from
 messages_to_clients.txt directly as is.

 Push a warning message to the client, it is a wrapper around:

 void push_warning_printf(
         THD *thd, Sql_condition::enum_warning_level level,
         uint code, const char *format, ...);
 */
void ib_senderrf(THD *thd,             /*!< in/out: session */
                 ib_log_level_t level, /*!< in: warning level */
                 uint32_t code,        /*!< MySQL error code */
                 ...);                 /*!< Args */

extern const char *TROUBLESHOOTING_MSG;
extern const char *TROUBLESHOOT_DATADICT_MSG;
extern const char *BUG_REPORT_MSG;
extern const char *FORCE_RECOVERY_MSG;
extern const char *ERROR_CREATING_MSG;
extern const char *OPERATING_SYSTEM_ERROR_MSG;
extern const char *FOREIGN_KEY_CONSTRAINTS_MSG;
extern const char *INNODB_PARAMETERS_MSG;

/** Returns the NUL terminated value of glob_hostname.
 @return pointer to glob_hostname. */
const char *server_get_hostname();

/** Get the error message format string.
 @return the format string or 0 if not found. */
const char *innobase_get_err_msg(int error_code); /*!< in: MySQL error code */

/** Compute the next autoinc value.

 For MySQL replication the autoincrement values can be partitioned among
 the nodes. The offset is the start or origin of the autoincrement value
 for a particular node. For n nodes the increment will be n and the offset
 will be in the interval [1, n]. The formula tries to allocate the next
 value for a particular node.

 Note: This function is also called with increment set to the number of
 values we want to reserve for multi-value inserts e.g.,

         INSERT INTO T VALUES(), (), ();

 innobase_next_autoinc() will be called with increment set to 3 where
 autoinc_lock_mode != TRADITIONAL because we want to reserve 3 values for
 the multi-value INSERT above.
 @return the next value */
[[nodiscard]] ulonglong innobase_next_autoinc(
    ulonglong current,    /*!< in: Current value */
    ulonglong need,       /*!< in: count of values needed */
    ulonglong step,       /*!< in: AUTOINC increment step */
    ulonglong offset,     /*!< in: AUTOINC offset */
    ulonglong max_value); /*!< in: max value for type */

/**********************************************************************
Check if the length of the identifier exceeds the maximum allowed.
The input to this function is an identifier in charset my_charset_filename.
return true when length of identifier is too long. */
bool innobase_check_identifier_length(
    const char *id); /* in: identifier to check.  it must belong
                     to charset my_charset_filename */

/**********************************************************************
Converts an identifier from my_charset_filename to UTF-8 charset. */
uint innobase_convert_to_system_charset(
    char *to,         /* out: converted identifier */
    const char *from, /* in: identifier to convert */
    ulint len,        /* in: length of 'to', in bytes */
    uint *errors);    /* out: error return */

/**********************************************************************
Converts an identifier from my_charset_filename to UTF-8 charset. */
uint innobase_convert_to_filename_charset(
    char *to,         /* out: converted identifier */
    const char *from, /* in: identifier to convert */
    ulint len);       /* in: length of 'to', in bytes */

/**********************************************************************
Issue a warning that the row is too big. */
void ib_warn_row_too_big(const dict_table_t *table);

/** InnoDB index push-down condition check defined in ha_innodb.cc
 @return ICP_NO_MATCH, ICP_MATCH, or ICP_OUT_OF_RANGE */

#include <my_icp.h>

[[nodiscard]] ICP_RESULT innobase_index_cond(
    ha_innobase *h); /*!< in/out: pointer to ha_innobase */

/** Gets information on the durability property requested by thread.
 Used when writing either a prepare or commit record to the log
 buffer.
 @return the durability property. */

#include <dur_prop.h>

[[nodiscard]] enum durability_properties thd_requested_durability(
    const THD *thd); /*!< in: thread handle */

/** Update the system variable with the given value of the InnoDB
buffer pool size.
@param[in]      buf_pool_size   given value of buffer pool size.*/
void innodb_set_buf_pool_size(long long buf_pool_size);

/** Gets the InnoDB transaction handle for a MySQL handler object, creates
an InnoDB transaction struct if the corresponding MySQL thread struct still
lacks one.
@param[in]      thd     MySQL thd (connection) object
@return InnoDB transaction handle */
trx_t *check_trx_exists(THD *thd);

/** Commits a transaction in an InnoDB database.
@param[in]      trx     Transaction handle. */
void innobase_commit_low(trx_t *trx);

/** Return the number of read threads for this session.
@param[in]      thd       Session instance, or nullptr to query the global
                          innodb_parallel_read_threads value. */
ulong thd_parallel_read_threads(THD *thd);

/** Return the maximum buffer size to use for DDL.
@param[in]      thd       Session instance, or nullptr to query the global
                          innodb_parallel_read_threads value.
@return memory upper limit in bytes. */
[[nodiscard]] ulong thd_ddl_buffer_size(THD *thd);

/** Whether this is a computed virtual column */
#define innobase_is_v_fld(field) ((field)->gcol_info && !(field)->stored_in_db)

/** @return the number of DDL threads to use (global/session). */
[[nodiscard]] size_t thd_ddl_threads(THD *thd) noexcept;

#endif /* HA_INNODB_PROTOTYPES_H */
