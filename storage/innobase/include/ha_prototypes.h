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

/*****************************************************************//**
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */

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

/**********************************************************************
Returns true if the thread is executing a SELECT statement. */

ibool
thd_is_select(
/*==========*/
				/* out: true if thd is executing SELECT */
	const void*	thd);	/* in: thread handle (THD*) */

/**********************************************************************
Check if the length of the identifier exceeds the maximum allowed.
The input to this function is an identifier in charset my_charset_filename.
return true when length of identifier is too long. */

my_bool
innobase_check_identifier_length(
/*=============================*/
	const char*	id);	/* in: identifier to check.  it must belong
				to charset my_charset_filename */

/**********************************************************************
Converts an identifier from my_charset_filename to UTF-8 charset. */

uint
innobase_convert_to_system_charset(
/*===============================*/
	char*		to,		/* out: converted identifier */
	const char*	from,		/* in: identifier to convert */
	ulint		len,		/* in: length of 'to', in bytes */
        uint*		errors);	/* out: error return */

/**********************************************************************
Converts an identifier from my_charset_filename to UTF-8 charset. */
uint
innobase_convert_to_filename_charset(
/*=================================*/
        char*           to,     /* out: converted identifier */
        const char*     from,   /* in: identifier to convert */
        ulint           len);   /* in: length of 'to', in bytes */

#endif
#endif
