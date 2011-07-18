/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

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

/**************************************************//**
@file include/trx0i_s.h
INFORMATION SCHEMA innodb_trx, innodb_locks and
innodb_lock_waits tables cache structures and public
functions.

Created July 17, 2007 Vasil Dimov
*******************************************************/

#ifndef trx0i_s_h
#define trx0i_s_h

#include "univ.i"
#include "trx0types.h"
#include "dict0types.h"
#include "ut0ut.h"

/** The maximum amount of memory that can be consumed by innodb_trx,
innodb_locks and innodb_lock_waits information schema tables. */
#define TRX_I_S_MEM_LIMIT		16777216 /* 16 MiB */

/** The maximum length of a string that can be stored in
i_s_locks_row_t::lock_data */
#define TRX_I_S_LOCK_DATA_MAX_LEN	8192

/** The maximum length of a string that can be stored in
i_s_trx_row_t::trx_query */
#define TRX_I_S_TRX_QUERY_MAX_LEN	1024

/** The maximum length of a string that can be stored in
i_s_trx_row_t::trx_operation_state */
#define TRX_I_S_TRX_OP_STATE_MAX_LEN	64

/** The maximum length of a string that can be stored in
i_s_trx_row_t::trx_foreign_key_error */
#define TRX_I_S_TRX_FK_ERROR_MAX_LEN	256

/** The maximum length of a string that can be stored in
i_s_trx_row_t::trx_isolation_level */
#define TRX_I_S_TRX_ISOLATION_LEVEL_MAX_LEN	16

/** Safely copy strings in to the INNODB_TRX table's
string based columns */
#define TRX_I_S_STRING_COPY(data, field, constraint, tcache)	\
do {								\
	if (strlen(data) > constraint) {			\
		char	buff[constraint + 1];			\
		strncpy(buff, data, constraint);		\
		buff[constraint] = '\0';			\
								\
		field = ha_storage_put_memlim(			\
			(tcache)->storage, buff, constraint + 1,\
			MAX_ALLOWED_FOR_STORAGE(tcache));	\
	} else {						\
		field = ha_storage_put_str_memlim(		\
			(tcache)->storage, data,		\
			MAX_ALLOWED_FOR_STORAGE(tcache));	\
	}							\
} while (0)

/** A row of INFORMATION_SCHEMA.innodb_locks */
typedef struct i_s_locks_row_struct	i_s_locks_row_t;
/** A row of INFORMATION_SCHEMA.innodb_trx */
typedef struct i_s_trx_row_struct i_s_trx_row_t;
/** A row of INFORMATION_SCHEMA.innodb_lock_waits */
typedef struct i_s_lock_waits_row_struct i_s_lock_waits_row_t;

/** Objects of trx_i_s_cache_t::locks_hash */
typedef struct i_s_hash_chain_struct	i_s_hash_chain_t;

/** Objects of this type are added to the hash table
trx_i_s_cache_t::locks_hash */
struct i_s_hash_chain_struct {
	i_s_locks_row_t*	value;	/*!< row of
					INFORMATION_SCHEMA.innodb_locks*/
	i_s_hash_chain_t*	next;	/*!< next item in the hash chain */
};

/** This structure represents INFORMATION_SCHEMA.innodb_locks row */
struct i_s_locks_row_struct {
	trx_id_t	lock_trx_id;	/*!< transaction identifier */
	const char*	lock_mode;	/*!< lock mode from
					lock_get_mode_str() */
	const char*	lock_type;	/*!< lock type from
					lock_get_type_str() */
	const char*	lock_table;	/*!< table name from
					lock_get_table_name() */
	const char*	lock_index;	/*!< index name from
					lock_rec_get_index_name() */
	/** Information for record locks.  All these are
	ULINT_UNDEFINED for table locks. */
	/* @{ */
	ulint		lock_space;	/*!< tablespace identifier */
	ulint		lock_page;	/*!< page number within the_space */
	ulint		lock_rec;	/*!< heap number of the record
					on the page */
	const char*	lock_data;	/*!< (some) content of the record */
	/* @} */

	/** The following are auxiliary and not included in the table */
	/* @{ */
	table_id_t	lock_table_id;
					/*!< table identifier from
					lock_get_table_id */
	i_s_hash_chain_t hash_chain;	/*!< hash table chain node for
					trx_i_s_cache_t::locks_hash */
	/* @} */
};

/** This structure represents INFORMATION_SCHEMA.innodb_trx row */
struct i_s_trx_row_struct {
	trx_id_t		trx_id;		/*!< transaction identifier */
	const char*		trx_state;	/*!< transaction state from
						trx_get_que_state_str() */
	ib_time_t		trx_started;	/*!< trx_struct::start_time */
	const i_s_locks_row_t*	requested_lock_row;
					/*!< pointer to a row
					in innodb_locks if trx
					is waiting, or NULL */
	ib_time_t	trx_wait_started; /*!< trx_struct::wait_started */
	ullint		trx_weight;	/*!< TRX_WEIGHT() */
	ulint		trx_mysql_thread_id; /*!< thd_get_thread_id() */
	const char*	trx_query;	/*!< MySQL statement being
					executed in the transaction */
	struct charset_info_st*	trx_query_cs;
					/*!< charset encode the MySQL
					statement */
	const char*	trx_operation_state; /*!< trx_struct::op_info */
	ulint		trx_tables_in_use;/*!< n_mysql_tables_in_use in
					 trx_struct */
	ulint		trx_tables_locked;
					/*!< mysql_n_tables_locked in
					trx_struct */
	ulint		trx_lock_structs;/*!< list len of trx_locks in
					trx_struct */
	ulint		trx_lock_memory_bytes;
					/*!< mem_heap_get_size(
					trx->lock_heap) */
	ulint		trx_rows_locked;/*!< lock_number_of_rows_locked() */
	ullint		trx_rows_modified;/*!< trx_struct::undo_no */
	ulint		trx_concurrency_tickets;
					/*!< n_tickets_to_enter_innodb in
					trx_struct */
	const char*	trx_isolation_level;
					/*!< isolation_level in trx_struct*/
	ibool		trx_unique_checks;
					/*!< check_unique_secondary in
					trx_struct*/
	ibool		trx_foreign_key_checks;
					/*!< check_foreigns in trx_struct */
	const char*	trx_foreign_key_error;
					/*!< detailed_error in trx_struct */
	ibool		trx_has_search_latch;
					/*!< has_search_latch in trx_struct */
	ulint		trx_search_latch_timeout;
					/*!< search_latch_timeout in
					trx_struct */
};

/** This structure represents INFORMATION_SCHEMA.innodb_lock_waits row */
struct i_s_lock_waits_row_struct {
	const i_s_locks_row_t*	requested_lock_row;	/*!< requested lock */
	const i_s_locks_row_t*	blocking_lock_row;	/*!< blocking lock */
};

/** Cache of INFORMATION_SCHEMA table data */
typedef struct trx_i_s_cache_struct	trx_i_s_cache_t;

/** Auxiliary enum used by functions that need to select one of the
INFORMATION_SCHEMA tables */
enum i_s_table {
	I_S_INNODB_TRX,		/*!< INFORMATION_SCHEMA.innodb_trx */
	I_S_INNODB_LOCKS,	/*!< INFORMATION_SCHEMA.innodb_locks */
	I_S_INNODB_LOCK_WAITS	/*!< INFORMATION_SCHEMA.innodb_lock_waits */
};

/** This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
extern trx_i_s_cache_t*	trx_i_s_cache;

/*******************************************************************//**
Initialize INFORMATION SCHEMA trx related cache. */
UNIV_INTERN
void
trx_i_s_cache_init(
/*===============*/
	trx_i_s_cache_t*	cache);	/*!< out: cache to init */
/*******************************************************************//**
Free the INFORMATION SCHEMA trx related cache. */
UNIV_INTERN
void
trx_i_s_cache_free(
/*===============*/
	trx_i_s_cache_t*	cache);	/*!< in/out: cache to free */

/*******************************************************************//**
Issue a shared/read lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_start_read(
/*=====================*/
	trx_i_s_cache_t*	cache);	/*!< in: cache */

/*******************************************************************//**
Release a shared/read lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_end_read(
/*===================*/
	trx_i_s_cache_t*	cache);	/*!< in: cache */

/*******************************************************************//**
Issue an exclusive/write lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_start_write(
/*======================*/
	trx_i_s_cache_t*	cache);	/*!< in: cache */

/*******************************************************************//**
Release an exclusive/write lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_end_write(
/*====================*/
	trx_i_s_cache_t*	cache);	/*!< in: cache */


/*******************************************************************//**
Retrieves the number of used rows in the cache for a given
INFORMATION SCHEMA table.
@return	number of rows */
UNIV_INTERN
ulint
trx_i_s_cache_get_rows_used(
/*========================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache */
	enum i_s_table		table);	/*!< in: which table */

/*******************************************************************//**
Retrieves the nth row in the cache for a given INFORMATION SCHEMA
table.
@return	row */
UNIV_INTERN
void*
trx_i_s_cache_get_nth_row(
/*======================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache */
	enum i_s_table		table,	/*!< in: which table */
	ulint			n);	/*!< in: row number */

/*******************************************************************//**
Update the transactions cache if it has not been read for some time.
@return	0 - fetched, 1 - not */
UNIV_INTERN
int
trx_i_s_possibly_fetch_data_into_cache(
/*===================================*/
	trx_i_s_cache_t*	cache);	/*!< in/out: cache */

/*******************************************************************//**
Returns TRUE if the data in the cache is truncated due to the memory
limit posed by TRX_I_S_MEM_LIMIT.
@return	TRUE if truncated */
UNIV_INTERN
ibool
trx_i_s_cache_is_truncated(
/*=======================*/
	trx_i_s_cache_t*	cache);	/*!< in: cache */

/** The maximum length of a resulting lock_id_size in
trx_i_s_create_lock_id(), not including the terminating NUL.
":%lu:%lu:%lu" -> 63 chars */
#define TRX_I_S_LOCK_ID_MAX_LEN	(TRX_ID_MAX_LEN + 63)

/*******************************************************************//**
Crafts a lock id string from a i_s_locks_row_t object. Returns its
second argument. This function aborts if there is not enough space in
lock_id. Be sure to provide at least TRX_I_S_LOCK_ID_MAX_LEN + 1 if you
want to be 100% sure that it will not abort.
@return	resulting lock id */
UNIV_INTERN
char*
trx_i_s_create_lock_id(
/*===================*/
	const i_s_locks_row_t*	row,	/*!< in: innodb_locks row */
	char*			lock_id,/*!< out: resulting lock_id */
	ulint			lock_id_size);/*!< in: size of the lock id
					buffer */

#endif /* trx0i_s_h */
