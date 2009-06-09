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

/******************************************************
INFORMATION SCHEMA innodb_trx, innodb_locks and
innodb_lock_waits tables cache structures and public
functions.

Created July 17, 2007 Vasil Dimov
*******************************************************/

#ifndef trx0i_s_h
#define trx0i_s_h

#include "univ.i"
#include "trx0types.h"
#include "ut0ut.h"

/* the maximum amount of memory that can be consumed by innodb_trx,
innodb_locks and innodb_lock_waits information schema tables. */
#define TRX_I_S_MEM_LIMIT		16777216 /* 16 MiB */

/* the maximum length of a string that can be stored in
i_s_locks_row_t::lock_data */
#define TRX_I_S_LOCK_DATA_MAX_LEN	8192

/* the maximum length of a string that can be stored in
i_s_trx_row_t::trx_query */
#define TRX_I_S_TRX_QUERY_MAX_LEN	1024

typedef struct i_s_locks_row_struct	i_s_locks_row_t;
typedef struct i_s_hash_chain_struct	i_s_hash_chain_t;

/* Objects of this type are added to the hash table
trx_i_s_cache_t::locks_hash */
struct i_s_hash_chain_struct {
	i_s_locks_row_t*	value;
	i_s_hash_chain_t*	next;
};

/* This structure represents INFORMATION_SCHEMA.innodb_locks row */
struct i_s_locks_row_struct {
	ullint		lock_trx_id;
	const char*	lock_mode;
	const char*	lock_type;
	const char*	lock_table;
	const char*	lock_index;
	ulint		lock_space;
	ulint		lock_page;
	ulint		lock_rec;
	const char*	lock_data;

	/* The following are auxiliary and not included in the table */
	ullint		lock_table_id;
	i_s_hash_chain_t hash_chain; /* this object is added to the hash
				    table
				    trx_i_s_cache_t::locks_hash */
};

/* This structure represents INFORMATION_SCHEMA.innodb_trx row */
typedef struct i_s_trx_row_struct {
	ullint			trx_id;
	const char*		trx_state;
	ib_time_t		trx_started;
	const i_s_locks_row_t*	requested_lock_row;
	ib_time_t		trx_wait_started;
	ullint			trx_weight;
	ulint			trx_mysql_thread_id;
	const char*		trx_query;
} i_s_trx_row_t;

/* This structure represents INFORMATION_SCHEMA.innodb_lock_waits row */
typedef struct i_s_lock_waits_row_struct {
	const i_s_locks_row_t*	requested_lock_row;
	const i_s_locks_row_t*	blocking_lock_row;
} i_s_lock_waits_row_t;

/* This type is opaque and is defined in trx/trx0i_s.c */
typedef struct trx_i_s_cache_struct	trx_i_s_cache_t;

/* Auxiliary enum used by functions that need to select one of the
INFORMATION_SCHEMA tables */
enum i_s_table {
	I_S_INNODB_TRX,
	I_S_INNODB_LOCKS,
	I_S_INNODB_LOCK_WAITS
};

/* This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
extern trx_i_s_cache_t*	trx_i_s_cache;

/***********************************************************************
Initialize INFORMATION SCHEMA trx related cache. */
UNIV_INTERN
void
trx_i_s_cache_init(
/*===============*/
	trx_i_s_cache_t*	cache);	/* out: cache to init */

/***********************************************************************
Issue a shared/read lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_start_read(
/*=====================*/
	trx_i_s_cache_t*	cache);	/* in: cache */

/***********************************************************************
Release a shared/read lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_end_read(
/*===================*/
	trx_i_s_cache_t*	cache);	/* in: cache */

/***********************************************************************
Issue an exclusive/write lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_start_write(
/*======================*/
	trx_i_s_cache_t*	cache);	/* in: cache */

/***********************************************************************
Release an exclusive/write lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_end_write(
/*====================*/
	trx_i_s_cache_t*	cache);	/* in: cache */


/***********************************************************************
Retrieves the number of used rows in the cache for a given
INFORMATION SCHEMA table. */
UNIV_INTERN
ulint
trx_i_s_cache_get_rows_used(
/*========================*/
					/* out: number of rows */
	trx_i_s_cache_t*	cache,	/* in: cache */
	enum i_s_table		table);	/* in: which table */

/***********************************************************************
Retrieves the nth row in the cache for a given INFORMATION SCHEMA
table. */
UNIV_INTERN
void*
trx_i_s_cache_get_nth_row(
/*======================*/
					/* out: row */
	trx_i_s_cache_t*	cache,	/* in: cache */
	enum i_s_table		table,	/* in: which table */
	ulint			n);	/* in: row number */

/***********************************************************************
Update the transactions cache if it has not been read for some time. */
UNIV_INTERN
int
trx_i_s_possibly_fetch_data_into_cache(
/*===================================*/
					/* out: 0 - fetched, 1 - not */
	trx_i_s_cache_t*	cache);	/* in/out: cache */

/***********************************************************************
Returns TRUE if the data in the cache is truncated due to the memory
limit posed by TRX_I_S_MEM_LIMIT. */
UNIV_INTERN
ibool
trx_i_s_cache_is_truncated(
/*=======================*/
					/* out: TRUE if truncated */
	trx_i_s_cache_t*	cache);	/* in: cache */

/* The maximum length of a resulting lock_id_size in
trx_i_s_create_lock_id(), not including the terminating '\0'.
":%lu:%lu:%lu" -> 63 chars */
#define TRX_I_S_LOCK_ID_MAX_LEN	(TRX_ID_MAX_LEN + 63)

/***********************************************************************
Crafts a lock id string from a i_s_locks_row_t object. Returns its
second argument. This function aborts if there is not enough space in
lock_id. Be sure to provide at least TRX_I_S_LOCK_ID_MAX_LEN + 1 if you
want to be 100% sure that it will not abort. */
UNIV_INTERN
char*
trx_i_s_create_lock_id(
/*===================*/
					/* out: resulting lock id */
	const i_s_locks_row_t*	row,	/* in: innodb_locks row */
	char*			lock_id,/* out: resulting lock_id */
	ulint			lock_id_size);/* in: size of the lock id
					buffer */

#endif /* trx0i_s_h */
