/*****************************************************************************

Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.

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

#ifndef HA_INNODB_PROTOTYPES_H
#define HA_INNODB_PROTOTYPES_H

#ifndef UNIV_HOTBACKUP

#include "univ.i" /* ulint, uint */
#include "m_ctype.h" /* CHARSET_INFO */

/* Prototypes for global functions in ha_innodb.cc that are called by
InnoDB's C-code. */

/*************************************************************************
Wrapper around MySQL's copy_and_convert function, see it for
documentation. */
UNIV_INTERN
ulint
innobase_convert_string(
/*====================*/
	void*		to,
	ulint		to_length,
	CHARSET_INFO*	to_cs,
	const void*	from,
	ulint		from_length,
	CHARSET_INFO*	from_cs,
	uint*		errors);

/***********************************************************************
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_(CHAR|VARCHAR|MYSQL|VARMYSQL) using "charset_coll" and writes
the result to "buf". The result is converted to "system_charset_info".
Not more than "buf_size" bytes are written to "buf".
The result is always '\0'-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating '\0'). */
UNIV_INTERN
ulint
innobase_raw_format(
/*================*/
					/* out: number of bytes
					that were written */
	const char*	data,		/* in: raw data */
	ulint		data_len,	/* in: raw data length
					in bytes */
	ulint		charset_coll,	/* in: charset collation */
	char*		buf,		/* out: output buffer */
	ulint		buf_size);	/* in: output buffer size
					in bytes */
                    
/*********************************************************************
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed. */
UNIV_INTERN
char*
innobase_convert_name(
/*==================*/
				/* out: pointer to the end of buf */
	char*		buf,	/* out: buffer for converted identifier */
	ulint		buflen,	/* in: length of buf, in bytes */
	const char*	id,	/* in: identifier to convert */
	ulint		idlen,	/* in: length of id, in bytes */
	void*		thd,	/* in: MySQL connection thread, or NULL */
	ibool		table_id);/* in: TRUE=id is a table or database name;
				FALSE=id is an index name */

/**********************************************************************
Returns true if the thread is the replication thread on the slave
server. Used in srv_conc_enter_innodb() to determine if the thread
should be allowed to enter InnoDB - the replication thread is treated
differently than other threads. Also used in
srv_conc_force_exit_innodb(). */
UNIV_INTERN
ibool
thd_is_replication_slave_thread(
/*============================*/
			/* out: true if thd is the replication thread */
	void*	thd);	/* in: thread handle (THD*) */

/**********************************************************************
Returns true if the transaction this thread is processing has edited
non-transactional tables. Used by the deadlock detector when deciding
which transaction to rollback in case of a deadlock - we try to avoid
rolling back transactions that have edited non-transactional tables. */
UNIV_INTERN
ibool
thd_has_edited_nontrans_tables(
/*===========================*/
			/* out: true if non-transactional tables have
			been edited */
	void*	thd);	/* in: thread handle (THD*) */

/*****************************************************************
Prints info of a THD object (== user session thread) to the given file. */
UNIV_INTERN
void
innobase_mysql_print_thd(
/*=====================*/
	FILE*	f,		/* in: output stream */
	void*	thd,		/* in: pointer to a MySQL THD object */
	uint	max_query_len);	/* in: max query length to print, or 0 to
				   use the default max length */

/******************************************************************
Converts a MySQL type to an InnoDB type. Note that this function returns
the 'mtype' of InnoDB. InnoDB differentiates between MySQL's old <= 4.1
VARCHAR and the new true VARCHAR in >= 5.0.3 by the 'prtype'. */
UNIV_INTERN
ulint
get_innobase_type_from_mysql_type(
/*==============================*/
					/* out: DATA_BINARY,
					DATA_VARCHAR, ... */
	ulint*		unsigned_flag,	/* out: DATA_UNSIGNED if an
					'unsigned type';
					at least ENUM and SET,
					and unsigned integer
					types are 'unsigned types' */
	const void*	field)		/* in: MySQL Field */
	__attribute__((nonnull));

/*****************************************************************
If you want to print a thd that is not associated with the current thread,
you must call this function before reserving the InnoDB kernel_mutex, to
protect MySQL from setting thd->query NULL. If you print a thd of the current
thread, we know that MySQL cannot modify thd->query, and it is not necessary
to call this. Call innobase_mysql_end_print_arbitrary_thd() after you release
the kernel_mutex. */
UNIV_INTERN
void
innobase_mysql_prepare_print_arbitrary_thd(void);
/*============================================*/

/*****************************************************************
Releases the mutex reserved by innobase_mysql_prepare_print_arbitrary_thd().
In the InnoDB latching order, the mutex sits right above the
kernel_mutex.  In debug builds, we assert that the kernel_mutex is
released before this function is invoked. */
UNIV_INTERN
void
innobase_mysql_end_print_arbitrary_thd(void);
/*========================================*/

/**********************************************************************
Get the variable length bounds of the given character set. */
UNIV_INTERN
void
innobase_get_cset_width(
/*====================*/
	ulint	cset,		/* in: MySQL charset-collation code */
	ulint*	mbminlen,	/* out: minimum length of a char (in bytes) */
	ulint*	mbmaxlen);	/* out: maximum length of a char (in bytes) */

/**********************************************************************
Compares NUL-terminated UTF-8 strings case insensitively. */
UNIV_INTERN
int
innobase_strcasecmp(
/*================*/
				/* out: 0 if a=b, <0 if a<b, >1 if a>b */
	const char*	a,	/* in: first string to compare */
	const char*	b);	/* in: second string to compare */

/**********************************************************************
Returns true if the thread is executing a SELECT statement. */

ibool
thd_is_select(
/*==========*/
				/* out: true if thd is executing SELECT */
	const void*	thd);	/* in: thread handle (THD*) */

/**********************************************************************
Converts an identifier to a table name. */
UNIV_INTERN
void
innobase_convert_from_table_id(
/*===========================*/
	struct charset_info_st*	cs,	/* in: the 'from' character set */
	char*			to,	/* out: converted identifier */
	const char*		from,	/* in: identifier to convert */
	ulint			len);	/* in: length of 'to', in bytes; should
					be at least 5 * strlen(to) + 1 */
/**********************************************************************
Converts an identifier to UTF-8. */
UNIV_INTERN
void
innobase_convert_from_id(
/*=====================*/
	struct charset_info_st*	cs,	/* in: the 'from' character set */
	char*			to,	/* out: converted identifier */
	const char*		from,	/* in: identifier to convert */
	ulint			len);	/* in: length of 'to', in bytes; should
					be at least 3 * strlen(to) + 1 */
/**********************************************************************
Makes all characters in a NUL-terminated UTF-8 string lower case. */
UNIV_INTERN
void
innobase_casedn_str(
/*================*/
	char*	a);	/* in/out: string to put in lower case */

/**************************************************************************
Determines the connection character set. */
struct charset_info_st*
innobase_get_charset(
/*=================*/
				/* out: connection character set */
	void*	mysql_thd);	/* in: MySQL thread handle */

/**********************************************************************
Returns true if the thread supports XA,
global value of innodb_supports_xa if thd is NULL. */

ibool
thd_supports_xa(
/*============*/
			/* out: true if thd supports XA */
	void*	thd);	/* in: thread handle (THD*), or NULL to query
			the global innodb_supports_xa */

/**********************************************************************
Returns the lock wait timeout for the current connection. */

ulong
thd_lock_wait_timeout(
/*==================*/
			/* out: the lock wait timeout, in seconds */
	void*	thd);	/* in: thread handle (THD*), or NULL to query
			the global innodb_lock_wait_timeout */

#endif
#endif
