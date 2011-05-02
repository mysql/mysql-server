/*****************************************************************************

Copyright (c) 2006, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/*******************************************************************//**
@file include/ha_prototypes.h
Prototypes for global functions in ha_innodb.cc that are called by
InnoDB C code

Created 5/11/2006 Osku Salerma
************************************************************************/

#ifndef HA_INNODB_PROTOTYPES_H
#define HA_INNODB_PROTOTYPES_H

#include "trx0types.h"
#include "m_ctype.h" /* CHARSET_INFO */

/*********************************************************************//**
Wrapper around MySQL's copy_and_convert function.
@return	number of bytes copied to 'to' */
UNIV_INTERN
ulint
innobase_convert_string(
/*====================*/
	void*		to,		/*!< out: converted string */
	ulint		to_length,	/*!< in: number of bytes reserved
					for the converted string */
	CHARSET_INFO*	to_cs,		/*!< in: character set to convert to */
	const void*	from,		/*!< in: string to convert */
	ulint		from_length,	/*!< in: number of bytes to convert */
	CHARSET_INFO*	from_cs,	/*!< in: character set to convert from */
	uint*		errors);	/*!< out: number of errors encountered
					during the conversion */

/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_(CHAR|VARCHAR|MYSQL|VARMYSQL) using "charset_coll" and writes
the result to "buf". The result is converted to "system_charset_info".
Not more than "buf_size" bytes are written to "buf".
The result is always NUL-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating NUL).
@return	number of bytes that were written */
UNIV_INTERN
ulint
innobase_raw_format(
/*================*/
	const char*	data,		/*!< in: raw data */
	ulint		data_len,	/*!< in: raw data length
					in bytes */
	ulint		charset_coll,	/*!< in: charset collation */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size);	/*!< in: output buffer size
					in bytes */

/*****************************************************************//**
Invalidates the MySQL query cache for the table. */
UNIV_INTERN
void
innobase_invalidate_query_cache(
/*============================*/
	trx_t*		trx,		/*!< in: transaction which
					modifies the table */
	const char*	full_name,	/*!< in: concatenation of
					database name, null char NUL,
					table name, null char NUL;
					NOTE that in Windows this is
					always in LOWER CASE! */
	ulint		full_name_len);	/*!< in: full name length where
					also the null chars count */

/*****************************************************************//**
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
UNIV_INTERN
char*
innobase_convert_name(
/*==================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	id,	/*!< in: identifier to convert */
	ulint		idlen,	/*!< in: length of id, in bytes */
	void*		thd,	/*!< in: MySQL connection thread, or NULL */
	ibool		table_id);/*!< in: TRUE=id is a table or database name;
				FALSE=id is an index name */

/******************************************************************//**
Returns true if the thread is the replication thread on the slave
server. Used in srv_conc_enter_innodb() to determine if the thread
should be allowed to enter InnoDB - the replication thread is treated
differently than other threads. Also used in
srv_conc_force_exit_innodb().
@return	true if thd is the replication thread */
UNIV_INTERN
ibool
thd_is_replication_slave_thread(
/*============================*/
	void*	thd);	/*!< in: thread handle (THD*) */

/******************************************************************//**
Returns true if the transaction this thread is processing has edited
non-transactional tables. Used by the deadlock detector when deciding
which transaction to rollback in case of a deadlock - we try to avoid
rolling back transactions that have edited non-transactional tables.
@return	true if non-transactional tables have been edited */
UNIV_INTERN
ibool
thd_has_edited_nontrans_tables(
/*===========================*/
	void*	thd);	/*!< in: thread handle (THD*) */

/*************************************************************//**
Prints info of a THD object (== user session thread) to the given file. */
UNIV_INTERN
void
innobase_mysql_print_thd(
/*=====================*/
	FILE*	f,		/*!< in: output stream */
	void*	thd,		/*!< in: pointer to a MySQL THD object */
	uint	max_query_len);	/*!< in: max query length to print, or 0 to
				   use the default max length */

/**************************************************************//**
Converts a MySQL type to an InnoDB type. Note that this function returns
the 'mtype' of InnoDB. InnoDB differentiates between MySQL's old <= 4.1
VARCHAR and the new true VARCHAR in >= 5.0.3 by the 'prtype'.
@return	DATA_BINARY, DATA_VARCHAR, ... */
UNIV_INTERN
ulint
get_innobase_type_from_mysql_type(
/*==============================*/
	ulint*		unsigned_flag,	/*!< out: DATA_UNSIGNED if an
					'unsigned type';
					at least ENUM and SET,
					and unsigned integer
					types are 'unsigned types' */
	const void*	field)		/*!< in: MySQL Field */
	__attribute__((nonnull));

/******************************************************************//**
Get the variable length bounds of the given character set. */
UNIV_INTERN
void
innobase_get_cset_width(
/*====================*/
	ulint	cset,		/*!< in: MySQL charset-collation code */
	ulint*	mbminlen,	/*!< out: minimum length of a char (in bytes) */
	ulint*	mbmaxlen);	/*!< out: maximum length of a char (in bytes) */

/******************************************************************//**
Compares NUL-terminated UTF-8 strings case insensitively.
@return	0 if a=b, <0 if a<b, >1 if a>b */
UNIV_INTERN
int
innobase_strcasecmp(
/*================*/
	const char*	a,	/*!< in: first string to compare */
	const char*	b);	/*!< in: second string to compare */

/******************************************************************//**
Strip dir name from a full path name and return only its file name.
@return file name or "null" if no file name */
UNIV_INTERN
const char*
innobase_basename(
/*==============*/
	const char*	path_name);	/*!< in: full path name */

/******************************************************************//**
Returns true if the thread is executing a SELECT statement.
@return	true if thd is executing SELECT */

ibool
thd_is_select(
/*==========*/
	const void*	thd);	/*!< in: thread handle (THD*) */

/******************************************************************//**
Converts an identifier to a table name. */
UNIV_INTERN
void
innobase_convert_from_table_id(
/*===========================*/
	struct charset_info_st*	cs,	/*!< in: the 'from' character set */
	char*			to,	/*!< out: converted identifier */
	const char*		from,	/*!< in: identifier to convert */
	ulint			len);	/*!< in: length of 'to', in bytes; should
					be at least 5 * strlen(to) + 1 */
/******************************************************************//**
Converts an identifier to UTF-8. */
UNIV_INTERN
void
innobase_convert_from_id(
/*=====================*/
	struct charset_info_st*	cs,	/*!< in: the 'from' character set */
	char*			to,	/*!< out: converted identifier */
	const char*		from,	/*!< in: identifier to convert */
	ulint			len);	/*!< in: length of 'to', in bytes; should
					be at least 3 * strlen(to) + 1 */
/******************************************************************//**
Makes all characters in a NUL-terminated UTF-8 string lower case. */
UNIV_INTERN
void
innobase_casedn_str(
/*================*/
	char*	a);	/*!< in/out: string to put in lower case */

/**********************************************************************//**
Determines the connection character set.
@return	connection character set */
UNIV_INTERN
struct charset_info_st*
innobase_get_charset(
/*=================*/
	void*	mysql_thd);	/*!< in: MySQL thread handle */
/**********************************************************************//**
Determines the current SQL statement.
@return	SQL statement string */
UNIV_INTERN
const char*
innobase_get_stmt(
/*==============*/
	void*	mysql_thd,	/*!< in: MySQL thread handle */
	size_t*	length)		/*!< out: length of the SQL statement */
	__attribute__((nonnull));
/******************************************************************//**
This function is used to find the storage length in bytes of the first n
characters for prefix indexes using a multibyte character set. The function
finds charset information and returns length of prefix_len characters in the
index field in bytes.
@return	number of bytes occupied by the first n characters */
UNIV_INTERN
ulint
innobase_get_at_most_n_mbchars(
/*===========================*/
	ulint charset_id,	/*!< in: character set id */
	ulint prefix_len,	/*!< in: prefix length in bytes of the index
				(this has to be divided by mbmaxlen to get the
				number of CHARACTERS n in the prefix) */
	ulint data_len,		/*!< in: length of the string in bytes */
	const char* str);	/*!< in: character string */

/******************************************************************//**
Returns true if the thread supports XA,
global value of innodb_supports_xa if thd is NULL.
@return	true if thd supports XA */

ibool
thd_supports_xa(
/*============*/
	void*	thd);	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_supports_xa */

/******************************************************************//**
Returns the lock wait timeout for the current connection.
@return	the lock wait timeout, in seconds */

ulong
thd_lock_wait_timeout(
/*==================*/
	void*	thd);	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_lock_wait_timeout */
/******************************************************************//**
Add up the time waited for the lock for the current query. */
UNIV_INTERN
void
thd_set_lock_wait_time(
/*===================*/
        void*   thd,	/*!< in: thread handle (THD*) */
        ulint   value);	/*!< in: time waited for the lock */

/**********************************************************************//**
Get the current setting of the lower_case_table_names global parameter from
mysqld.cc. We do a dirty read because for one there is no synchronization
object and secondly there is little harm in doing so even if we get a torn
read.
@return	value of lower_case_table_names */
UNIV_INTERN
ulint
innobase_get_lower_case_table_names(void);
/*=====================================*/

#endif
