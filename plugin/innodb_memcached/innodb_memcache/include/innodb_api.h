/***********************************************************************

Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/
/**************************************************//**
@file innodb_api.h

Created 03/15/2011      Jimmy Yang
*******************************************************/

#ifndef innodb_api_h
#define innodb_api_h

#include "api0api.h"
#include "innodb_engine.h"
#include "assert.h"
#include "handler_api.h"

/** Macros to lock/unlock the engine connection mutex */
#define LOCK_CONN_IF_NOT_LOCKED(has_lock, engine)		\
	if (!(has_lock)) {					\
		pthread_mutex_lock(&(engine)->conn_mutex);	\
	}

#define UNLOCK_CONN_IF_NOT_LOCKED(has_lock, engine)		\
	if (!(has_lock)) {					\
		pthread_mutex_unlock(&(engine)->conn_mutex);	\
	}

/** Macros to lock/unlock the connection mutex, used for any
connection specific operations */
#define LOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn)		\
	if (!(has_lock)) {					\
		pthread_mutex_lock(&(conn)->curr_conn_mutex);	\
	}

#define LOCK_CURRENT_CONN_TRYLOCK(conn)				\
               pthread_mutex_trylock(&(conn)->curr_conn_mutex); \

#define UNLOCK_CURRENT_CONN_IF_NOT_LOCKED(has_lock, conn)	\
	if (!(has_lock)) {					\
		pthread_mutex_unlock(&(conn)->curr_conn_mutex);	\
	}

/** We would need to fetch 5 column values from each key value rows if they
are available. They are the "key", "value", "cas", "exp" and "flag" */
#define	MCI_COL_TO_GET		5

typedef enum mci_col {
	MCI_COL_KEY,		/*!< key */
	MCI_COL_VALUE,		/*!< value */
	MCI_COL_FLAG,		/*!< flag */
	MCI_COL_CAS,		/*!< check and set value */
	MCI_COL_EXP		/*!< expiration */
} mci_col_t;

/** mci_column is the structure that stores and describes a column info
in InnoDB Memcached. The supported column types are either character
type or integer type (see above "enum mci_col" for columns
supporting memcached) */
typedef struct mci_column {
	char*		value_str;	/*!< char value of the column */
	int		value_len;	/*!< char value length in bytes */
	uint64_t	value_int;	/*!< integer value */
	bool		is_str;		/*!< whether the value is char or int */
	bool		is_unsigned;	/*!< whether the value is signed or not */
	bool		is_valid;	/*!< this structure contains valid
					or stale column value */
	bool		is_null;	/*!< whether it is a NULL value */
	bool		allocated;	/*!< whether memory allocated to store
					the value */
} mci_column_t;

/** "mci_item_t" represents values we read from a table row, and enough
to assemble to an memcached response. As described in above mci_col,
we must have "MCI_COL_TO_GET" (5) column values to read. In addition,
the user table could have multiple "value" columns, and it is possible
to map such multiple "value" columns to a single memcached key,
such value is separated by "separator" as defined in the "config_option"
table. And we will assemble and disassemble the memcached value from these
column values. And "extra_col_value" and "n_extra_col" is used to support
multiple value columns */
typedef struct mci_item {
	mci_column_t	col_value[MCI_COL_TO_GET]; /*!< columns in a row */
	mci_column_t*	extra_col_value;	/*!< whether there will be
						additional/multiple "values"
						to be stored */
	int		n_extra_col;		/*!< number of additional
						"value" columns */
} mci_item_t;


/*************************************************************//**
Register InnoDB Callback functions */
void
register_innodb_cb(
/*===============*/
	void*	p);		/*!<in: Pointer to callback function arrary */

/*********************************************************************
Open a table and return a cursor for the table. */
ib_err_t
innodb_api_begin(
/*=============*/
	innodb_engine_t*
			engine,		/*!< in: InnoDB Memcached engine */
	const char*	dbname,		/*!< in: database name */
	const char*	name,		/*!< in: table name */
	innodb_conn_data_t* conn_data,	/*!< in/out: connnection specific
					data */
	ib_trx_t	ib_trx,		/*!< in: transaction */
	ib_crsr_t*	crsr,		/*!< out: innodb cursor */
	ib_crsr_t*	idx_crsr,	/*!< out: innodb index cursor */
	ib_lck_mode_t	lock_mode);	/*!< in:  lock mode */

/*************************************************************//**
Position a row accord to key, and fetch value if needed
@return DB_SUCCESS if successful otherwise, error code */
ib_err_t
innodb_api_search(
/*==============*/
	innodb_conn_data_t*	cursor_data,/*!< in/out: cursor info */
	ib_crsr_t*		crsr,	/*!< in/out: cursor used to seacrh */
	const char*		key,	/*!< in: key to search */
	int			len,	/*!< in: key length */
	mci_item_t*		item,	/*!< in: result */
	ib_tpl_t*		r_tpl,	/*!< in: tpl for other DML operations */
	bool			sel_only); /*!< in: for select only */

/*************************************************************//**
Insert a row
@return DB_SUCCESS if successful otherwise, error code */
ib_err_t
innodb_api_insert(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in/out: cursor info */
	const char*		key,	/*!< in: value to insert */
	int			len,	/*!< in: value length */
	uint32_t		val_len,/*!< in: value length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< in/out: cas value */
        uint64_t		flags);	/*!< in: flags */

/*************************************************************//**
Delete a row, implements the "remove" command
@return ENGINE_SUCCESS if successful otherwise, error code */
ENGINE_ERROR_CODE
innodb_api_delete(
/*==============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in/out: cursor info */
	const char*		key,	/*!< in: value to insert */
	int			len);	/*!< in: value length */

/*************************************************************//**
Update a row with arithmetic operation
@return DB_SUCCESS if successful otherwise ENGINE_NOT_STORED*/
ENGINE_ERROR_CODE
innodb_api_arithmetic(
/*==================*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in/out: cursor info */
	const char*		key,	/*!< in: key values */
	int			len,	/*!< in: key length */
	int			delta,	/*!< in: value to add or subtract */
	bool			increment,/*!< in: increment or decrement */
	uint64_t*		cas,	/*!< out: cas */
	rel_time_t		exp_time, /*!< in: expire time */
	bool			create,	/*!< in: whether to create new entry
					if not found */
	uint64_t		initial,/*!< in: initialize value */
	uint64_t*		result);/*!< out: result value */

/*************************************************************//**
This is the interface to following commands:
	1) add
	2) replace
	3) append
	4) prepend
	5) set
	6) cas
@return ENGINE_SUCCESS if successful otherwise, error code */
ENGINE_ERROR_CODE
innodb_api_store(
/*=============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	cursor_data,/*!< in/out: cursor info */
	const char*		key,	/*!< in: key value */
	int			len,	/*!< in: key length */
	uint32_t		val_len,/*!< in: value length */
	uint64_t		exp,	/*!< in: expire time */
	uint64_t*		cas,	/*!< out: cas value */
	uint64_t		input_cas,/*!< in: cas value supplied by user */
	uint64_t		flags,	/*!< in: flags */
	ENGINE_STORE_OPERATION	op);	/*!< in: Operations */

/*********************************************************************
Implement the "flush_all" command, map to InnoDB's trunk table operation
return ENGINE_SUCCESS is all successful */
ENGINE_ERROR_CODE
innodb_api_flush(
/*=============*/
	innodb_engine_t*	engine,	/*!< in: InnoDB Memcached engine */
	innodb_conn_data_t*	conn_data,/*!< in/out: cursor affiliated
					with a connection */
	const char*		dbname,	/*!< in: database name */
	const char*		name);	/*!< in: table name */

/*************************************************************//**
Get current time */
uint64_t
mci_get_time(void);
/*==============*/

/** types of operations performed */
typedef enum conn_op_type {
	CONN_OP_READ,		/*!< read operation */
	CONN_OP_WRITE,		/*!< write operation */
	CONN_OP_DELETE,		/*!< delete operation */
	CONN_OP_FLUSH		/*!< flush operation */
} conn_op_type_t;

/*************************************************************//**
Increment read and write counters, if they exceed the batch size,
commit the transaction. */
void
innodb_api_cursor_reset(
/*====================*/
	innodb_engine_t*	engine,		 /*!< in: InnoDB Memcached
						engine */
	innodb_conn_data_t*	conn_data,	/*!< in/out: cursor affiliated
						with a connection */
	conn_op_type_t		op_type,	/*!< in: type of DML
						performed */
	bool			commit);	/*!< in: commit or abort trx */

/*************************************************************//**
Increment read and write counters, if they exceed the batch size,
commit the transaction. */
bool
innodb_reset_conn(
/*==============*/
	innodb_conn_data_t*	conn_data,	/*!< in/out: cursor affiliated
						with a connection */
	bool			has_lock,	/*!< in: has lock on
						connection */
	bool			commit,		/*!< in: commit or abort trx */
	bool			has_binlog);	/*!< in: binlog enabled */

/**********************************************************************//**
This function verifies the table configuration information on an opened
table, and fills in columns used for memcached functionalities (cas, exp etc.)
@return true if everything works out fine */
ib_err_t
innodb_verify_low(
/*==============*/
        meta_cfg_info_t*	info,		/*!< in/out: meta info
						structure */
        ib_crsr_t		crsr,		/*!< in: crsr */
	bool			runtime);	/*!< in: verify at the
						runtime */

/*************************************************************//**
Following are a set of InnoDB callback function wrappers for functions
that will be used outside innodb_api.c
@return DB_SUCCESS if successful or error code */
ib_err_t
innodb_cb_cursor_close(
/*===================*/
	ib_crsr_t	ib_crsr);	/*!< in: cursor to close */

/*************************************************************//**
Commit the transaction
@return DB_SUCCESS if successful or error code */
ib_err_t
innodb_cb_trx_commit(
/*=================*/
	ib_trx_t	ib_trx);	/*!< in: transaction to commit */

/*************************************************************//**
Close table associated to the connection
@return DB_SUCCESS if successful or error code */
ib_err_t
innodb_cb_close_thd(
/*=================*/
	void*		thd);		/*!< in: THD */

/*****************************************************************//**
update the cursor with new transactions and also reset the cursor
@return DB_SUCCESS or error code */
ib_err_t
innodb_cb_cursor_new_trx(
/*=====================*/
	ib_crsr_t	ib_crsr,	/*!< in/out: InnoDB cursor */
	ib_trx_t	ib_trx);	/*!< in: transaction */

/*****************************************************************//**
Lock the table with specified lock mode
@return DB_SUCCESS or error code */
ib_err_t
innodb_cb_cursor_lock(
/*==================*/
	innodb_engine_t* eng,		/*!< in: InnoDB Memcached engine */
	ib_crsr_t	ib_crsr,	/*!< in/out: cursor on the table */
	ib_lck_mode_t	ib_lck_mode);	/*!< in: lock mode */

/*****************************************************************//**
Create an InnoDB tuple used for index/table search.
@return own: Tuple for current index */
ib_tpl_t
innodb_cb_read_tuple_create(
/*========================*/
	ib_crsr_t	ib_crsr);	/*!< in: Cursor instance */

/*****************************************************************//**
Move cursor to the first record in the table.
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_cursor_first(
/*===================*/
	ib_crsr_t	ib_crsr);	/*!< in: InnoDB cursor instance */

/*****************************************************************//**
Read current row.
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_read_row(
/*===============*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_tpl_t	ib_tpl);	/*!< out: read cols into this tuple */

/*****************************************************************//**
Get a column type, length and attributes from the tuple.
@return len of column data */
ib_ulint_t
innodb_cb_col_get_meta(
/*===================*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i,		/*!< in: column index in tuple */
	ib_col_meta_t*	ib_col_meta);	/*!< out: column meta data */

/*****************************************************************//**
Destroy an InnoDB tuple. */
void
innodb_cb_tuple_delete(
/*===================*/
        ib_tpl_t	ib_tpl);	/*!< in,own: Tuple instance to delete */

/*****************************************************************//**
Return the number of columns in the tuple definition.
@return number of columns */
ib_ulint_t
innodb_cb_tuple_get_n_cols(
/*=======================*/
	const ib_tpl_t	ib_tpl);	/*!< in: Tuple for table/index */

/*****************************************************************//**
Get a column value pointer from the tuple.
@return NULL or pointer to buffer */
const void*
innodb_cb_col_get_value(
/*====================*/
	ib_tpl_t	ib_tpl,		/*!< in: tuple instance */
	ib_ulint_t	i);		/*!< in: column index in tuple */

/********************************************************************//**
Open a table using the table name.
@return table instance if found */
ib_err_t
innodb_cb_open_table(
/*=================*/
	const char*	name,		/*!< in: table name to lookup */
	ib_trx_t	ib_trx,		/*!< in: transaction */
	ib_crsr_t*	ib_crsr);	/*!< in: cursor to be used */

/*****************************************************************//**
Get a column name from the tuple.
@return name of the column */
char*
innodb_cb_col_get_name(
/*===================*/
	ib_crsr_t	ib_crsr,	/*!< in: InnoDB cursor instance */
	ib_ulint_t	i);		/*!< in: column index in tuple */

/*****************************************************************//**
Open an InnoDB secondary index cursor and return a cursor handle to it.
@return DB_SUCCESS or err code */
ib_err_t
innodb_cb_cursor_open_index_using_name(
/*===================================*/
	ib_crsr_t	ib_open_crsr,	/*!< in: open/active cursor */
	const char*	index_name,	/*!< in: secondary index name */
	ib_crsr_t*	ib_crsr,	/*!< out,own: InnoDB index cursor */
	int*		idx_type,	/*!< out: index is cluster index */
	ib_id_u64_t*	idx_id);	/*!< out: index id */

/*****************************************************************//**
Get InnoDB API configure option
@return configure status */
int
innodb_cb_get_cfg();
/*===============*/
#endif
