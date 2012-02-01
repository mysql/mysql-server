/*****************************************************************************

Copyright (c) 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************************/

/**************************************************//**
@file handler_api.h
Interface to MySQL Handler functions, currently used
for binlogging

Created 3/14/2011 Jimmy Yang
*******************************************************/

#ifndef HANDLER_API_H
#define HANDLER_API_H

#define MYSQL_SERVER 1

#include <my_global.h>
#include <sql_priv.h>
#include <stdlib.h>
#include <ctype.h>
#include <mysql_version.h>
#include <mysql/plugin.h>
#include <my_dir.h>
#include "my_pthread.h"
#include "my_sys.h"
#include "m_string.h"
#include "sql_plugin.h"
#include "table.h"
#include "sql_class.h"
#include <sql_base.h>
#include "key.h"
#include "lock.h"
#include "transaction.h"
#include "sql_handler.h"
#include "handler.h"

/** Defines for handler_unlock_table()'s mode field */
#define HDL_READ	0x1
#define HDL_WRITE	0x2

/** Defines for handler_binlog_row()'s mode field */
#define HDL_UPDATE	1
#define HDL_INSERT	2
#define HDL_DELETE	3

extern "C" {
/**********************************************************************//**
Creates a THD object.
@return a pointer to the THD object, NULL if failed */
void*
handler_create_thd(void);
/*====================*/

/**********************************************************************//**
Creates a MySQL TABLE object with specified database name and table name.
@return a pointer to the TABLE object, NULL if does not exist */
void*
handler_open_table(
/*===============*/
	void*		thd,		/*!< in: THD* */
	const char*	db_name,	/*!< in: database name */
	const char*	table_name,	/*!< in: table name */
	int		lock_mode);	/*!< in: lock mode */

/**********************************************************************//**
Wrapper of function binlog_log_row() to binlog an operation on a row */
void
handler_binlog_row(
/*===============*/
	void*		my_table,	/*!< in: Table metadata */
	int		mode);		/*!< in: type of DML */

/**********************************************************************//**
Flush binlog from cache to binlog file */
void
handler_binlog_flush(
/*=================*/
	void*		my_thd,		/*!< in: THD* */
	void*		my_table);	/*!< in: TABLE structure */

/**********************************************************************//**
Reset TABLE->record[0] */
void
handler_rec_init(
	void*		table);		/*!< in: Table metadata */

/**********************************************************************//**
Set up a char based field in TABLE->record[0] */
void
handler_rec_setup_str(
/*==================*/
	void*		table,		/*!< in/out: TABLE structure */
	int		field_id,	/*!< in: Field ID for the field */
	const char*		str,		/*!< in: string to set */
	int		len);		/*!< in: length of string */

/**********************************************************************//**
Set up an integer field in TABLE->record[0] */
void
handler_rec_setup_int(
/*==================*/
        void*		table,		/*!< in/out: TABLE structure */
        int		field_id,	/*!< in: Field ID for the field */
        int		value,		/*!< in: value to set */
	bool		unsigned_flag,	/*!< in: whether it is unsigned */
	bool		is_null);	/*!< in: whether it is null value */
/**********************************************************************//**
Unlock a table and commit the transaction
return 0 if fail to commit the transaction */
int
handler_unlock_table(
/*=================*/
	void*			my_thd,		/*!< in: thread */
	void*			my_table,	/*!< in: Table metadata */
	int			mode);		/*!< in: mode */
/**********************************************************************//**
close an handler */
void
handler_close_thd(
/*==============*/
	void*			my_thd);	/*!< in: thread */
/**********************************************************************//**
copy an record */
void
handler_store_record(
/*=================*/
	void*			table);		/*!< in: TABLE */

/**********************************************************************//**
binlog a truncate table statement */
void
handler_binlog_truncate(
/*====================*/
	void*			my_thd,		/*!< in: THD* */
	char*			table_name);	/*!< in: table name */
}

/**********************************************************************//**
binlog a row operation */
extern
int
binlog_log_row(
/*===========*/
	TABLE*		table,		/*!< in: ptr to TABLE */
	const uchar	*before_record,	/*!< in: Before image of record */
	const uchar	*after_record,	/*!< in: Current image of record */
	Log_func*	log_func);	/*!< in: Log function */

/** function to close a connection and thd, defined in sql/handler.cc */
extern void ha_close_connection(THD* thd);

/**********************************************************************
Following APIs  can perform DMLs through MySQL handler interface. They
are currently disabled and under HANDLER_API_MEMCACHED define
**********************************************************************/

#ifdef HANDLER_API_MEMCACHED

/** structure holds the search field(s) */
typedef struct field_arg {
	unsigned int	num_arg;
	int*		len;
	char**		value;
} field_arg_t;

/** Macros to create and instantiate fields */
#define MCI_ADD_FIELD(m_args, m_fld, m_value, m_len)			\
	do {								\
		(m_args)->len[m_fld] = m_len;				\
		(m_args)->value[m_fld] = (char*)(m_value);		\
	} while(0)

#define MCI_CREATE_FIELD(field, num_fld)				\
	do {								\
		field->len = (int*)malloc(num_fld * sizeof(*(field->len)));\
		memset(field->len, 0, num_fld * sizeof(*(field->len)));	\
		field->value = (char**)malloc(num_fld			\
					      * sizeof(*(field->value)));\
		field->num_arg = num_fld;				\
	} while(0)

#define MCI_FREE_FIELD(field)						\
	do {								\
		free(field->len);					\
		free(field->value);					\
		field->num_arg = 0;					\
	} while(0)

/**********************************************************************//**
Search table for a record with particular search criteria
@return a pointer to table->record[0] */
uchar*
handler_select_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: TABLE structure */
	field_arg_t*	srch_args,	/*!< in: field to search */
	int		idx_to_use);	/*!< in: index to use */

/**********************************************************************//**
Insert a record to the table
return 0 if successfully inserted */
int
handler_insert_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: TABLE structure */
	field_arg_t*	store_args);	/*!< in: inserting row data */

/**********************************************************************//**
Update a record
return 0 if successfully inserted */
int
handler_update_rec(
/*===============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table,		/*!< in: TABLE structure */
	field_arg_t*	store_args);	/*!< in: update row data */

/**********************************************************************//**
Delete a record
return 0 if successfully inserted */
int
handler_delete_rec(
/*===============*/
        THD*		thd,		/*!< in: thread */
        TABLE*		table);		/*!< in: TABLE structure */

/**********************************************************************//**
Lock a table
return A lock structure pointer on success, NULL on error */
MYSQL_LOCK *
handler_lock_table(
/*===============*/
	THD*			thd,		/*!< in: thread */
	TABLE*			table,		/*!< in: Table metadata */
	enum thr_lock_type	lock_mode);	/*!< in: lock mode */

#endif /* HANDLER_API_MEMCACHED */

#endif /* HANDLER_API_H */

