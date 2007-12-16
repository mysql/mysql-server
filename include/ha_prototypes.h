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

/*********************************************************************
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed. */

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

ibool
thd_has_edited_nontrans_tables(
/*===========================*/
			/* out: true if non-transactional tables have
			been edited */
	void*	thd);	/* in: thread handle (THD*) */

/*****************************************************************
Prints info of a THD object (== user session thread) to the given file. */

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

void
innobase_mysql_prepare_print_arbitrary_thd(void);
/*============================================*/

/*****************************************************************
Releases the mutex reserved by innobase_mysql_prepare_print_arbitrary_thd().
*/

void
innobase_mysql_end_print_arbitrary_thd(void);
/*========================================*/

#endif
#endif
