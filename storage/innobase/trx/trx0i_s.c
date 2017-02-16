/*****************************************************************************

Copyright (c) 2007, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file trx/trx0i_s.c
INFORMATION SCHEMA innodb_trx, innodb_locks and
innodb_lock_waits tables fetch code.

The code below fetches information needed to fill those
3 dynamic tables and uploads it into a "transactions
table cache" for later retrieval.

Created July 17, 2007 Vasil Dimov
*******************************************************/

/* Found during the build of 5.5.3 on Linux 2.4 and early 2.6 kernels:
   The includes "univ.i" -> "my_global.h" cause a different path
   to be taken further down with pthread functions and types,
   so they must come first.
   From the symptoms, this is related to bug#46587 in the MySQL bug DB.
*/
#include "univ.i"

#include <mysql/plugin.h>

#include "buf0buf.h"
#include "dict0dict.h"
#include "ha0storage.h"
#include "ha_prototypes.h"
#include "hash0hash.h"
#include "lock0iter.h"
#include "lock0lock.h"
#include "mem0mem.h"
#include "page0page.h"
#include "rem0rec.h"
#include "row0row.h"
#include "srv0srv.h"
#include "sync0rw.h"
#include "sync0sync.h"
#include "sync0types.h"
#include "trx0i_s.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "ut0mem.h"
#include "ut0ut.h"

/** Initial number of rows in the table cache */
#define TABLE_CACHE_INITIAL_ROWSNUM	1024

/** @brief The maximum number of chunks to allocate for a table cache.

The rows of a table cache are stored in a set of chunks. When a new
row is added a new chunk is allocated if necessary. Assuming that the
first one is 1024 rows (TABLE_CACHE_INITIAL_ROWSNUM) and each
subsequent is N/2 where N is the number of rows we have allocated till
now, then 39th chunk would accommodate 1677416425 rows and all chunks
would accommodate 3354832851 rows. */
#define MEM_CHUNKS_IN_TABLE_CACHE	39

/** The following are some testing auxiliary macros. Do not enable them
in a production environment. */
/* @{ */

#if 0
/** If this is enabled then lock folds will always be different
resulting in equal rows being put in a different cells of the hash
table. Checking for duplicates will be flawed because different
fold will be calculated when a row is searched in the hash table. */
#define TEST_LOCK_FOLD_ALWAYS_DIFFERENT
#endif

#if 0
/** This effectively kills the search-for-duplicate-before-adding-a-row
function, but searching in the hash is still performed. It will always
be assumed that lock is not present and insertion will be performed in
the hash table. */
#define TEST_NO_LOCKS_ROW_IS_EVER_EQUAL_TO_LOCK_T
#endif

#if 0
/** This aggressively repeats adding each row many times. Depending on
the above settings this may be noop or may result in lots of rows being
added. */
#define TEST_ADD_EACH_LOCKS_ROW_MANY_TIMES
#endif

#if 0
/** Very similar to TEST_NO_LOCKS_ROW_IS_EVER_EQUAL_TO_LOCK_T but hash
table search is not performed at all. */
#define TEST_DO_NOT_CHECK_FOR_DUPLICATE_ROWS
#endif

#if 0
/** Do not insert each row into the hash table, duplicates may appear
if this is enabled, also if this is enabled searching into the hash is
noop because it will be empty. */
#define TEST_DO_NOT_INSERT_INTO_THE_HASH_TABLE
#endif
/* @} */

/** Memory limit passed to ha_storage_put_memlim().
@param cache	hash storage
@return		maximum allowed allocation size */
#define MAX_ALLOWED_FOR_STORAGE(cache)		\
	(TRX_I_S_MEM_LIMIT			\
	 - (cache)->mem_allocd)

/** Memory limit in table_cache_create_empty_row().
@param cache	hash storage
@return		maximum allowed allocation size */
#define MAX_ALLOWED_FOR_ALLOC(cache)		\
	(TRX_I_S_MEM_LIMIT			\
	 - (cache)->mem_allocd			\
	 - ha_storage_get_size((cache)->storage))

/** Memory for each table in the intermediate buffer is allocated in
separate chunks. These chunks are considered to be concatenated to
represent one flat array of rows. */
typedef struct i_s_mem_chunk_struct {
	ulint	offset;		/*!< offset, in number of rows */
	ulint	rows_allocd;	/*!< the size of this chunk, in number
				of rows */
	void*	base;		/*!< start of the chunk */
} i_s_mem_chunk_t;

/** This represents one table's cache. */
typedef struct i_s_table_cache_struct {
	ulint		rows_used;	/*!< number of used rows */
	ulint		rows_allocd;	/*!< number of allocated rows */
	ulint		row_size;	/*!< size of a single row */
	i_s_mem_chunk_t	chunks[MEM_CHUNKS_IN_TABLE_CACHE]; /*!< array of
					memory chunks that stores the
					rows */
} i_s_table_cache_t;

/** This structure describes the intermediate buffer */
struct trx_i_s_cache_struct {
	rw_lock_t	rw_lock;	/*!< read-write lock protecting
					the rest of this structure */
	ullint		last_read;	/*!< last time the cache was read;
					measured in microseconds since
					epoch */
	i_s_table_cache_t innodb_trx;	/*!< innodb_trx table */
	i_s_table_cache_t innodb_locks;	/*!< innodb_locks table */
	i_s_table_cache_t innodb_lock_waits;/*!< innodb_lock_waits table */
/** the hash table size is LOCKS_HASH_CELLS_NUM * sizeof(void*) bytes */
#define LOCKS_HASH_CELLS_NUM		10000
	hash_table_t*	locks_hash;	/*!< hash table used to eliminate
					duplicate entries in the
					innodb_locks table */
/** Initial size of the cache storage */
#define CACHE_STORAGE_INITIAL_SIZE	1024
/** Number of hash cells in the cache storage */
#define CACHE_STORAGE_HASH_CELLS	2048
	ha_storage_t*	storage;	/*!< storage for external volatile
					data that can possibly not be
					available later, when we release
					the kernel mutex */
	ulint		mem_allocd;	/*!< the amount of memory
					allocated with mem_alloc*() */
	ibool		is_truncated;	/*!< this is TRUE if the memory
					limit was hit and thus the data
					in the cache is truncated */
};

/** This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
static trx_i_s_cache_t	trx_i_s_cache_static;
/** This is the intermediate buffer where data needed to fill the
INFORMATION SCHEMA tables is fetched and later retrieved by the C++
code in handler/i_s.cc. */
UNIV_INTERN trx_i_s_cache_t*	trx_i_s_cache = &trx_i_s_cache_static;

/* Key to register the lock/mutex with performance schema */
#ifdef UNIV_PFS_RWLOCK
UNIV_INTERN mysql_pfs_key_t	trx_i_s_cache_lock_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	cache_last_read_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/*******************************************************************//**
For a record lock that is in waiting state retrieves the only bit that
is set, for a table lock returns ULINT_UNDEFINED.
@return	record number within the heap */
static
ulint
wait_lock_get_heap_no(
/*==================*/
	const lock_t*	lock)	/*!< in: lock */
{
	ulint	ret;

	switch (lock_get_type(lock)) {
	case LOCK_REC:
		ret = lock_rec_find_set_bit(lock);
		ut_a(ret != ULINT_UNDEFINED);
		break;
	case LOCK_TABLE:
		ret = ULINT_UNDEFINED;
		break;
	default:
		ut_error;
	}

	return(ret);
}

/*******************************************************************//**
Initializes the members of a table cache. */
static
void
table_cache_init(
/*=============*/
	i_s_table_cache_t*	table_cache,	/*!< out: table cache */
	size_t			row_size)	/*!< in: the size of a
						row */
{
	ulint	i;

	table_cache->rows_used = 0;
	table_cache->rows_allocd = 0;
	table_cache->row_size = row_size;

	for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {

		/* the memory is actually allocated in
		table_cache_create_empty_row() */
		table_cache->chunks[i].base = NULL;
	}
}

/*******************************************************************//**
Frees a table cache. */
static
void
table_cache_free(
/*=============*/
	i_s_table_cache_t*	table_cache)	/*!< in/out: table cache */
{
	ulint	i;

	for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {

		/* the memory is actually allocated in
		table_cache_create_empty_row() */
		if (table_cache->chunks[i].base) {
			mem_free(table_cache->chunks[i].base);
			table_cache->chunks[i].base = NULL;
		}
	}
}

/*******************************************************************//**
Returns an empty row from a table cache. The row is allocated if no more
empty rows are available. The number of used rows is incremented.
If the memory limit is hit then NULL is returned and nothing is
allocated.
@return	empty row, or NULL if out of memory */
static
void*
table_cache_create_empty_row(
/*=========================*/
	i_s_table_cache_t*	table_cache,	/*!< in/out: table cache */
	trx_i_s_cache_t*	cache)		/*!< in/out: cache to record
						how many bytes are
						allocated */
{
	ulint	i;
	void*	row;

	ut_a(table_cache->rows_used <= table_cache->rows_allocd);

	if (table_cache->rows_used == table_cache->rows_allocd) {

		/* rows_used == rows_allocd means that new chunk needs
		to be allocated: either no more empty rows in the
		last allocated chunk or nothing has been allocated yet
		(rows_num == rows_allocd == 0); */

		i_s_mem_chunk_t*	chunk;
		ulint			req_bytes;
		ulint			got_bytes;
		ulint			req_rows;
		ulint			got_rows;

		/* find the first not allocated chunk */
		for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {

			if (table_cache->chunks[i].base == NULL) {

				break;
			}
		}

		/* i == MEM_CHUNKS_IN_TABLE_CACHE means that all chunks
		have been allocated :-X */
		ut_a(i < MEM_CHUNKS_IN_TABLE_CACHE);

		/* allocate the chunk we just found */

		if (i == 0) {

			/* first chunk, nothing is allocated yet */
			req_rows = TABLE_CACHE_INITIAL_ROWSNUM;
		} else {

			/* Memory is increased by the formula
			new = old + old / 2; We are trying not to be
			aggressive here (= using the common new = old * 2)
			because the allocated memory will not be freed
			until InnoDB exit (it is reused). So it is better
			to once allocate the memory in more steps, but
			have less unused/wasted memory than to use less
			steps in allocation (which is done once in a
			lifetime) but end up with lots of unused/wasted
			memory. */
			req_rows = table_cache->rows_allocd / 2;
		}
		req_bytes = req_rows * table_cache->row_size;

		if (req_bytes > MAX_ALLOWED_FOR_ALLOC(cache)) {

			return(NULL);
		}

		chunk = &table_cache->chunks[i];

		chunk->base = mem_alloc2(req_bytes, &got_bytes);

		got_rows = got_bytes / table_cache->row_size;

		cache->mem_allocd += got_bytes;

#if 0
		printf("allocating chunk %d req bytes=%lu, got bytes=%lu, "
		       "row size=%lu, "
		       "req rows=%lu, got rows=%lu\n",
		       i, req_bytes, got_bytes,
		       table_cache->row_size,
		       req_rows, got_rows);
#endif

		chunk->rows_allocd = got_rows;

		table_cache->rows_allocd += got_rows;

		/* adjust the offset of the next chunk */
		if (i < MEM_CHUNKS_IN_TABLE_CACHE - 1) {

			table_cache->chunks[i + 1].offset
				= chunk->offset + chunk->rows_allocd;
		}

		/* return the first empty row in the newly allocated
		chunk */
		row = chunk->base;
	} else {

		char*	chunk_start;
		ulint	offset;

		/* there is an empty row, no need to allocate new
		chunks */

		/* find the first chunk that contains allocated but
		empty/unused rows */
		for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {

			if (table_cache->chunks[i].offset
			    + table_cache->chunks[i].rows_allocd
			    > table_cache->rows_used) {

				break;
			}
		}

		/* i == MEM_CHUNKS_IN_TABLE_CACHE means that all chunks
		are full, but
		table_cache->rows_used != table_cache->rows_allocd means
		exactly the opposite - there are allocated but
		empty/unused rows :-X */
		ut_a(i < MEM_CHUNKS_IN_TABLE_CACHE);

		chunk_start = (char*) table_cache->chunks[i].base;
		offset = table_cache->rows_used
			- table_cache->chunks[i].offset;

		row = chunk_start + offset * table_cache->row_size;
	}

	table_cache->rows_used++;

	return(row);
}

#ifdef UNIV_DEBUG
/*******************************************************************//**
Validates a row in the locks cache.
@return	TRUE if valid */
static
ibool
i_s_locks_row_validate(
/*===================*/
	const i_s_locks_row_t*	row)	/*!< in: row to validate */
{
	ut_ad(row->lock_trx_id != 0);
	ut_ad(row->lock_mode != NULL);
	ut_ad(row->lock_type != NULL);
	ut_ad(row->lock_table != NULL);
	ut_ad(row->lock_table_id != 0);

	if (row->lock_space == ULINT_UNDEFINED) {
		/* table lock */
		ut_ad(!strcmp("TABLE", row->lock_type));
		ut_ad(row->lock_index == NULL);
		ut_ad(row->lock_data == NULL);
		ut_ad(row->lock_page == ULINT_UNDEFINED);
		ut_ad(row->lock_rec == ULINT_UNDEFINED);
	} else {
		/* record lock */
		ut_ad(!strcmp("RECORD", row->lock_type));
		ut_ad(row->lock_index != NULL);
		/* row->lock_data == NULL if buf_page_try_get() == NULL */
		ut_ad(row->lock_page != ULINT_UNDEFINED);
		ut_ad(row->lock_rec != ULINT_UNDEFINED);
	}

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/*******************************************************************//**
Fills i_s_trx_row_t object.
If memory can not be allocated then FALSE is returned.
@return	FALSE if allocation fails */
static
ibool
fill_trx_row(
/*=========*/
	i_s_trx_row_t*		row,		/*!< out: result object
						that's filled */
	const trx_t*		trx,		/*!< in: transaction to
						get data from */
	const i_s_locks_row_t*	requested_lock_row,/*!< in: pointer to the
						corresponding row in
						innodb_locks if trx is
						waiting or NULL if trx
						is not waiting */
	trx_i_s_cache_t*	cache)		/*!< in/out: cache into
						which to copy volatile
						strings */
{
	const char*	stmt;
	size_t		stmt_len;
	const char*	s;

	ut_ad(mutex_own(&kernel_mutex));

	row->trx_id = trx->id;
	row->trx_started = (ib_time_t) trx->start_time;
	row->trx_state = trx_get_que_state_str(trx);
	row->requested_lock_row = requested_lock_row;
	ut_ad(requested_lock_row == NULL
	      || i_s_locks_row_validate(requested_lock_row));

	if (trx->wait_lock != NULL) {
		ut_a(requested_lock_row != NULL);
		row->trx_wait_started = (ib_time_t) trx->wait_started;
	} else {
		ut_a(requested_lock_row == NULL);
		row->trx_wait_started = 0;
	}

	row->trx_weight = (ullint) TRX_WEIGHT(trx);

	if (trx->mysql_thd == NULL) {
		/* For internal transactions e.g., purge and transactions
		being recovered at startup there is no associated MySQL
		thread data structure. */
		row->trx_mysql_thread_id = 0;
		row->trx_query = NULL;
		goto thd_done;
	}

	row->trx_mysql_thread_id = thd_get_thread_id(trx->mysql_thd);
	stmt = innobase_get_stmt(trx->mysql_thd, &stmt_len);

	if (stmt != NULL) {
		char	query[TRX_I_S_TRX_QUERY_MAX_LEN + 1];

		if (stmt_len > TRX_I_S_TRX_QUERY_MAX_LEN) {
			stmt_len = TRX_I_S_TRX_QUERY_MAX_LEN;
		}

		memcpy(query, stmt, stmt_len);
		query[stmt_len] = '\0';

		row->trx_query = ha_storage_put_memlim(
			cache->storage, query, stmt_len + 1,
			MAX_ALLOWED_FOR_STORAGE(cache));

		row->trx_query_cs = innobase_get_charset(trx->mysql_thd);

		if (row->trx_query == NULL) {

			return(FALSE);
		}
	} else {

		row->trx_query = NULL;
	}

thd_done:
	s = trx->op_info;

	if (s != NULL && s[0] != '\0') {

		TRX_I_S_STRING_COPY(s, row->trx_operation_state,
				    TRX_I_S_TRX_OP_STATE_MAX_LEN, cache);

		if (row->trx_operation_state == NULL) {

			return(FALSE);
		}
	} else {

		row->trx_operation_state = NULL;
	}

	row->trx_tables_in_use = trx->n_mysql_tables_in_use;

	row->trx_tables_locked = trx->mysql_n_tables_locked;

	row->trx_lock_structs = UT_LIST_GET_LEN(trx->trx_locks);

	row->trx_lock_memory_bytes = mem_heap_get_size(trx->lock_heap);

	row->trx_rows_locked = lock_number_of_rows_locked(trx);

	row->trx_rows_modified = trx->undo_no;

	row->trx_concurrency_tickets = trx->n_tickets_to_enter_innodb;

	switch (trx->isolation_level) {
	case TRX_ISO_READ_UNCOMMITTED:
		row->trx_isolation_level = "READ UNCOMMITTED";
		break;
	case TRX_ISO_READ_COMMITTED:
		row->trx_isolation_level = "READ COMMITTED";
		break;
	case TRX_ISO_REPEATABLE_READ:
		row->trx_isolation_level = "REPEATABLE READ";
		break;
	case TRX_ISO_SERIALIZABLE:
		row->trx_isolation_level = "SERIALIZABLE";
		break;
	/* Should not happen as TRX_ISO_READ_COMMITTED is default */
	default:
		row->trx_isolation_level = "UNKNOWN";
	}

	row->trx_unique_checks = (ibool) trx->check_unique_secondary;

	row->trx_foreign_key_checks = (ibool) trx->check_foreigns;

	s = trx->detailed_error;

	if (s != NULL && s[0] != '\0') {

		TRX_I_S_STRING_COPY(s,
				    row->trx_foreign_key_error,
				    TRX_I_S_TRX_FK_ERROR_MAX_LEN, cache);

		if (row->trx_foreign_key_error == NULL) {

			return(FALSE);
		}
	} else {
		row->trx_foreign_key_error = NULL;
	}

	row->trx_has_search_latch = (ibool) trx->has_search_latch;

	row->trx_search_latch_timeout = trx->search_latch_timeout;

	return(TRUE);
}

/*******************************************************************//**
Format the nth field of "rec" and put it in "buf". The result is always
NUL-terminated. Returns the number of bytes that were written to "buf"
(including the terminating NUL).
@return	end of the result */
static
ulint
put_nth_field(
/*==========*/
	char*			buf,	/*!< out: buffer */
	ulint			buf_size,/*!< in: buffer size in bytes */
	ulint			n,	/*!< in: number of field */
	const dict_index_t*	index,	/*!< in: index */
	const rec_t*		rec,	/*!< in: record */
	const ulint*		offsets)/*!< in: record offsets, returned
					by rec_get_offsets() */
{
	const byte*	data;
	ulint		data_len;
	dict_field_t*	dict_field;
	ulint		ret;

	ut_ad(rec_offs_validate(rec, NULL, offsets));

	if (buf_size == 0) {

		return(0);
	}

	ret = 0;

	if (n > 0) {
		/* we must append ", " before the actual data */

		if (buf_size < 3) {

			buf[0] = '\0';
			return(1);
		}

		memcpy(buf, ", ", 3);

		buf += 2;
		buf_size -= 2;
		ret += 2;
	}

	/* now buf_size >= 1 */

	data = rec_get_nth_field(rec, offsets, n, &data_len);

	dict_field = dict_index_get_nth_field(index, n);

	ret += row_raw_format((const char*) data, data_len,
			      dict_field, buf, buf_size);

	return(ret);
}

/*******************************************************************//**
Fills the "lock_data" member of i_s_locks_row_t object.
If memory can not be allocated then FALSE is returned.
@return	FALSE if allocation fails */
static
ibool
fill_lock_data(
/*===========*/
	const char**		lock_data,/*!< out: "lock_data" to fill */
	const lock_t*		lock,	/*!< in: lock used to find the data */
	ulint			heap_no,/*!< in: rec num used to find the data */
	trx_i_s_cache_t*	cache)	/*!< in/out: cache where to store
					volatile data */
{
	mtr_t			mtr;

	const buf_block_t*	block;
	const page_t*		page;
	const rec_t*		rec;

	ut_a(lock_get_type(lock) == LOCK_REC);

	mtr_start(&mtr);

	block = buf_page_try_get(lock_rec_get_space_id(lock),
				 lock_rec_get_page_no(lock),
				 &mtr);

	if (block == NULL) {

		*lock_data = NULL;

		mtr_commit(&mtr);

		return(TRUE);
	}

	page = (const page_t*) buf_block_get_frame(block);

	rec = page_find_rec_with_heap_no(page, heap_no);

	if (page_rec_is_infimum(rec)) {

		*lock_data = ha_storage_put_str_memlim(
			cache->storage, "infimum pseudo-record",
			MAX_ALLOWED_FOR_STORAGE(cache));
	} else if (page_rec_is_supremum(rec)) {

		*lock_data = ha_storage_put_str_memlim(
			cache->storage, "supremum pseudo-record",
			MAX_ALLOWED_FOR_STORAGE(cache));
	} else {

		const dict_index_t*	index;
		ulint			n_fields;
		mem_heap_t*		heap;
		ulint			offsets_onstack[REC_OFFS_NORMAL_SIZE];
		ulint*			offsets;
		char			buf[TRX_I_S_LOCK_DATA_MAX_LEN];
		ulint			buf_used;
		ulint			i;

		rec_offs_init(offsets_onstack);
		offsets = offsets_onstack;

		index = lock_rec_get_index(lock);

		n_fields = dict_index_get_n_unique(index);

		ut_a(n_fields > 0);

		heap = NULL;
		offsets = rec_get_offsets(rec, index, offsets, n_fields,
					  &heap);

		/* format and store the data */

		buf_used = 0;
		for (i = 0; i < n_fields; i++) {

			buf_used += put_nth_field(
				buf + buf_used, sizeof(buf) - buf_used,
				i, index, rec, offsets) - 1;
		}

		*lock_data = (const char*) ha_storage_put_memlim(
			cache->storage, buf, buf_used + 1,
			MAX_ALLOWED_FOR_STORAGE(cache));

		if (UNIV_UNLIKELY(heap != NULL)) {

			/* this means that rec_get_offsets() has created a new
			heap and has stored offsets in it; check that this is
			really the case and free the heap */
			ut_a(offsets != offsets_onstack);
			mem_heap_free(heap);
		}
	}

	mtr_commit(&mtr);

	if (*lock_data == NULL) {

		return(FALSE);
	}

	return(TRUE);
}

/*******************************************************************//**
Fills i_s_locks_row_t object. Returns its first argument.
If memory can not be allocated then FALSE is returned.
@return	FALSE if allocation fails */
static
ibool
fill_locks_row(
/*===========*/
	i_s_locks_row_t* row,	/*!< out: result object that's filled */
	const lock_t*	lock,	/*!< in: lock to get data from */
	ulint		heap_no,/*!< in: lock's record number
				or ULINT_UNDEFINED if the lock
				is a table lock */
	trx_i_s_cache_t* cache)	/*!< in/out: cache into which to copy
				volatile strings */
{
	row->lock_trx_id = lock_get_trx_id(lock);
	row->lock_mode = lock_get_mode_str(lock);
	row->lock_type = lock_get_type_str(lock);

	row->lock_table = ha_storage_put_str_memlim(
		cache->storage, lock_get_table_name(lock),
		MAX_ALLOWED_FOR_STORAGE(cache));

	/* memory could not be allocated */
	if (row->lock_table == NULL) {

		return(FALSE);
	}

	switch (lock_get_type(lock)) {
	case LOCK_REC:
		row->lock_index = ha_storage_put_str_memlim(
			cache->storage, lock_rec_get_index_name(lock),
			MAX_ALLOWED_FOR_STORAGE(cache));

		/* memory could not be allocated */
		if (row->lock_index == NULL) {

			return(FALSE);
		}

		row->lock_space = lock_rec_get_space_id(lock);
		row->lock_page = lock_rec_get_page_no(lock);
		row->lock_rec = heap_no;

		if (!fill_lock_data(&row->lock_data, lock, heap_no, cache)) {

			/* memory could not be allocated */
			return(FALSE);
		}

		break;
	case LOCK_TABLE:
		row->lock_index = NULL;

		row->lock_space = ULINT_UNDEFINED;
		row->lock_page = ULINT_UNDEFINED;
		row->lock_rec = ULINT_UNDEFINED;

		row->lock_data = NULL;

		break;
	default:
		ut_error;
	}

	row->lock_table_id = lock_get_table_id(lock);

	row->hash_chain.value = row;
	ut_ad(i_s_locks_row_validate(row));

	return(TRUE);
}

/*******************************************************************//**
Fills i_s_lock_waits_row_t object. Returns its first argument.
@return	result object that's filled */
static
i_s_lock_waits_row_t*
fill_lock_waits_row(
/*================*/
	i_s_lock_waits_row_t*	row,		/*!< out: result object
						that's filled */
	const i_s_locks_row_t*	requested_lock_row,/*!< in: pointer to the
						relevant requested lock
						row in innodb_locks */
	const i_s_locks_row_t*	blocking_lock_row)/*!< in: pointer to the
						relevant blocking lock
						row in innodb_locks */
{
	ut_ad(i_s_locks_row_validate(requested_lock_row));
	ut_ad(i_s_locks_row_validate(blocking_lock_row));

	row->requested_lock_row = requested_lock_row;
	row->blocking_lock_row = blocking_lock_row;

	return(row);
}

/*******************************************************************//**
Calculates a hash fold for a lock. For a record lock the fold is
calculated from 4 elements, which uniquely identify a lock at a given
point in time: transaction id, space id, page number, record number.
For a table lock the fold is table's id.
@return	fold */
static
ulint
fold_lock(
/*======*/
	const lock_t*	lock,	/*!< in: lock object to fold */
	ulint		heap_no)/*!< in: lock's record number
				or ULINT_UNDEFINED if the lock
				is a table lock */
{
#ifdef TEST_LOCK_FOLD_ALWAYS_DIFFERENT
	static ulint	fold = 0;

	return(fold++);
#else
	ulint	ret;

	switch (lock_get_type(lock)) {
	case LOCK_REC:
		ut_a(heap_no != ULINT_UNDEFINED);

		ret = ut_fold_ulint_pair((ulint) lock_get_trx_id(lock),
					 lock_rec_get_space_id(lock));

		ret = ut_fold_ulint_pair(ret,
					 lock_rec_get_page_no(lock));

		ret = ut_fold_ulint_pair(ret, heap_no);

		break;
	case LOCK_TABLE:
		/* this check is actually not necessary for continuing
		correct operation, but something must have gone wrong if
		it fails. */
		ut_a(heap_no == ULINT_UNDEFINED);

		ret = (ulint) lock_get_table_id(lock);

		break;
	default:
		ut_error;
	}

	return(ret);
#endif
}

/*******************************************************************//**
Checks whether i_s_locks_row_t object represents a lock_t object.
@return	TRUE if they match */
static
ibool
locks_row_eq_lock(
/*==============*/
	const i_s_locks_row_t*	row,	/*!< in: innodb_locks row */
	const lock_t*		lock,	/*!< in: lock object */
	ulint			heap_no)/*!< in: lock's record number
					or ULINT_UNDEFINED if the lock
					is a table lock */
{
	ut_ad(i_s_locks_row_validate(row));
#ifdef TEST_NO_LOCKS_ROW_IS_EVER_EQUAL_TO_LOCK_T
	return(0);
#else
	switch (lock_get_type(lock)) {
	case LOCK_REC:
		ut_a(heap_no != ULINT_UNDEFINED);

		return(row->lock_trx_id == lock_get_trx_id(lock)
		       && row->lock_space == lock_rec_get_space_id(lock)
		       && row->lock_page == lock_rec_get_page_no(lock)
		       && row->lock_rec == heap_no);

	case LOCK_TABLE:
		/* this check is actually not necessary for continuing
		correct operation, but something must have gone wrong if
		it fails. */
		ut_a(heap_no == ULINT_UNDEFINED);

		return(row->lock_trx_id == lock_get_trx_id(lock)
		       && row->lock_table_id == lock_get_table_id(lock));

	default:
		ut_error;
		return(FALSE);
	}
#endif
}

/*******************************************************************//**
Searches for a row in the innodb_locks cache that has a specified id.
This happens in O(1) time since a hash table is used. Returns pointer to
the row or NULL if none is found.
@return	row or NULL */
static
i_s_locks_row_t*
search_innodb_locks(
/*================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache */
	const lock_t*		lock,	/*!< in: lock to search for */
	ulint			heap_no)/*!< in: lock's record number
					or ULINT_UNDEFINED if the lock
					is a table lock */
{
	i_s_hash_chain_t*	hash_chain;

	HASH_SEARCH(
		/* hash_chain->"next" */
		next,
		/* the hash table */
		cache->locks_hash,
		/* fold */
		fold_lock(lock, heap_no),
		/* the type of the next variable */
		i_s_hash_chain_t*,
		/* auxiliary variable */
		hash_chain,
		/* assertion on every traversed item */
		ut_ad(i_s_locks_row_validate(hash_chain->value)),
		/* this determines if we have found the lock */
		locks_row_eq_lock(hash_chain->value, lock, heap_no));

	if (hash_chain == NULL) {

		return(NULL);
	}
	/* else */

	return(hash_chain->value);
}

/*******************************************************************//**
Adds new element to the locks cache, enlarging it if necessary.
Returns a pointer to the added row. If the row is already present then
no row is added and a pointer to the existing row is returned.
If row can not be allocated then NULL is returned.
@return	row */
static
i_s_locks_row_t*
add_lock_to_cache(
/*==============*/
	trx_i_s_cache_t*	cache,	/*!< in/out: cache */
	const lock_t*		lock,	/*!< in: the element to add */
	ulint			heap_no)/*!< in: lock's record number
					or ULINT_UNDEFINED if the lock
					is a table lock */
{
	i_s_locks_row_t*	dst_row;

#ifdef TEST_ADD_EACH_LOCKS_ROW_MANY_TIMES
	ulint	i;
	for (i = 0; i < 10000; i++) {
#endif
#ifndef TEST_DO_NOT_CHECK_FOR_DUPLICATE_ROWS
	/* quit if this lock is already present */
	dst_row = search_innodb_locks(cache, lock, heap_no);
	if (dst_row != NULL) {

		ut_ad(i_s_locks_row_validate(dst_row));
		return(dst_row);
	}
#endif

	dst_row = (i_s_locks_row_t*)
		table_cache_create_empty_row(&cache->innodb_locks, cache);

	/* memory could not be allocated */
	if (dst_row == NULL) {

		return(NULL);
	}

	if (!fill_locks_row(dst_row, lock, heap_no, cache)) {

		/* memory could not be allocated */
		cache->innodb_locks.rows_used--;
		return(NULL);
	}

#ifndef TEST_DO_NOT_INSERT_INTO_THE_HASH_TABLE
	HASH_INSERT(
		/* the type used in the hash chain */
		i_s_hash_chain_t,
		/* hash_chain->"next" */
		next,
		/* the hash table */
		cache->locks_hash,
		/* fold */
		fold_lock(lock, heap_no),
		/* add this data to the hash */
		&dst_row->hash_chain);
#endif
#ifdef TEST_ADD_EACH_LOCKS_ROW_MANY_TIMES
	} /* for()-loop */
#endif

	ut_ad(i_s_locks_row_validate(dst_row));
	return(dst_row);
}

/*******************************************************************//**
Adds new pair of locks to the lock waits cache.
If memory can not be allocated then FALSE is returned.
@return	FALSE if allocation fails */
static
ibool
add_lock_wait_to_cache(
/*===================*/
	trx_i_s_cache_t*	cache,		/*!< in/out: cache */
	const i_s_locks_row_t*	requested_lock_row,/*!< in: pointer to the
						relevant requested lock
						row in innodb_locks */
	const i_s_locks_row_t*	blocking_lock_row)/*!< in: pointer to the
						relevant blocking lock
						row in innodb_locks */
{
	i_s_lock_waits_row_t*	dst_row;

	dst_row = (i_s_lock_waits_row_t*)
		table_cache_create_empty_row(&cache->innodb_lock_waits,
					     cache);

	/* memory could not be allocated */
	if (dst_row == NULL) {

		return(FALSE);
	}

	fill_lock_waits_row(dst_row, requested_lock_row, blocking_lock_row);

	return(TRUE);
}

/*******************************************************************//**
Adds transaction's relevant (important) locks to cache.
If the transaction is waiting, then the wait lock is added to
innodb_locks and a pointer to the added row is returned in
requested_lock_row, otherwise requested_lock_row is set to NULL.
If rows can not be allocated then FALSE is returned and the value of
requested_lock_row is undefined.
@return	FALSE if allocation fails */
static
ibool
add_trx_relevant_locks_to_cache(
/*============================*/
	trx_i_s_cache_t*	cache,	/*!< in/out: cache */
	const trx_t*		trx,	/*!< in: transaction */
	i_s_locks_row_t**	requested_lock_row)/*!< out: pointer to the
					requested lock row, or NULL or
					undefined */
{
	ut_ad(mutex_own(&kernel_mutex));

	/* If transaction is waiting we add the wait lock and all locks
	from another transactions that are blocking the wait lock. */
	if (trx->que_state == TRX_QUE_LOCK_WAIT) {

		const lock_t*		curr_lock;
		ulint			wait_lock_heap_no;
		i_s_locks_row_t*	blocking_lock_row;
		lock_queue_iterator_t	iter;

		ut_a(trx->wait_lock != NULL);

		wait_lock_heap_no
			= wait_lock_get_heap_no(trx->wait_lock);

		/* add the requested lock */
		*requested_lock_row
			= add_lock_to_cache(cache, trx->wait_lock,
					    wait_lock_heap_no);

		/* memory could not be allocated */
		if (*requested_lock_row == NULL) {

			return(FALSE);
		}

		/* then iterate over the locks before the wait lock and
		add the ones that are blocking it */

		lock_queue_iterator_reset(&iter, trx->wait_lock,
					  ULINT_UNDEFINED);

		curr_lock = lock_queue_iterator_get_prev(&iter);
		while (curr_lock != NULL) {

			if (lock_has_to_wait(trx->wait_lock,
					     curr_lock)) {

				/* add the lock that is
				blocking trx->wait_lock */
				blocking_lock_row
					= add_lock_to_cache(
						cache, curr_lock,
						/* heap_no is the same
						for the wait and waited
						locks */
						wait_lock_heap_no);

				/* memory could not be allocated */
				if (blocking_lock_row == NULL) {

					return(FALSE);
				}

				/* add the relation between both locks
				to innodb_lock_waits */
				if (!add_lock_wait_to_cache(
						cache, *requested_lock_row,
						blocking_lock_row)) {

					/* memory could not be allocated */
					return(FALSE);
				}
			}

			curr_lock = lock_queue_iterator_get_prev(&iter);
		}
	} else {

		*requested_lock_row = NULL;
	}

	return(TRUE);
}

/** The minimum time that a cache must not be updated after it has been
read for the last time; measured in microseconds. We use this technique
to ensure that SELECTs which join several INFORMATION SCHEMA tables read
the same version of the cache. */
#define CACHE_MIN_IDLE_TIME_US	100000 /* 0.1 sec */

/*******************************************************************//**
Checks if the cache can safely be updated.
@return	TRUE if can be updated */
static
ibool
can_cache_be_updated(
/*=================*/
	trx_i_s_cache_t*	cache)	/*!< in: cache */
{
	ullint	now;

#ifdef UNIV_SYNC_DEBUG
	ut_a(rw_lock_own(&cache->rw_lock, RW_LOCK_EX));
#endif

	now = ut_time_us(NULL);
	if (now - cache->last_read > CACHE_MIN_IDLE_TIME_US) {

		return(TRUE);
	}

	return(FALSE);
}

/*******************************************************************//**
Declare a cache empty, preparing it to be filled up. Not all resources
are freed because they can be reused. */
static
void
trx_i_s_cache_clear(
/*================*/
	trx_i_s_cache_t*	cache)	/*!< out: cache to clear */
{
	cache->innodb_trx.rows_used = 0;
	cache->innodb_locks.rows_used = 0;
	cache->innodb_lock_waits.rows_used = 0;

	hash_table_clear(cache->locks_hash);

	ha_storage_empty(&cache->storage);
}

/*******************************************************************//**
Fetches the data needed to fill the 3 INFORMATION SCHEMA tables into the
table cache buffer. Cache must be locked for write. */
static
void
fetch_data_into_cache(
/*==================*/
	trx_i_s_cache_t*	cache)	/*!< in/out: cache */
{
	trx_t*			trx;
	i_s_trx_row_t*		trx_row;
	i_s_locks_row_t*	requested_lock_row;

	ut_ad(mutex_own(&kernel_mutex));

	trx_i_s_cache_clear(cache);

	/* We iterate over the list of all transactions and add each one
	to innodb_trx's cache. We also add all locks that are relevant
	to each transaction into innodb_locks' and innodb_lock_waits'
	caches. */

	for (trx = UT_LIST_GET_FIRST(trx_sys->trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		if (!add_trx_relevant_locks_to_cache(cache, trx,
						     &requested_lock_row)) {

			cache->is_truncated = TRUE;
			return;
		}

		trx_row = (i_s_trx_row_t*)
			table_cache_create_empty_row(&cache->innodb_trx,
						     cache);

		/* memory could not be allocated */
		if (trx_row == NULL) {

			cache->is_truncated = TRUE;
			return;
		}

		if (!fill_trx_row(trx_row, trx, requested_lock_row, cache)) {

			/* memory could not be allocated */
			cache->innodb_trx.rows_used--;
			cache->is_truncated = TRUE;
			return;
		}
	}

	cache->is_truncated = FALSE;
}

/*******************************************************************//**
Update the transactions cache if it has not been read for some time.
Called from handler/i_s.cc.
@return	0 - fetched, 1 - not */
UNIV_INTERN
int
trx_i_s_possibly_fetch_data_into_cache(
/*===================================*/
	trx_i_s_cache_t*	cache)	/*!< in/out: cache */
{
	ullint	now;

#ifdef UNIV_SYNC_DEBUG
	ut_a(rw_lock_own(&cache->rw_lock, RW_LOCK_EX));
#endif

	if (!can_cache_be_updated(cache)) {

		return(1);
	}

	/* We need to read trx_sys and record/table lock queues */
	mutex_enter(&kernel_mutex);

	fetch_data_into_cache(cache);

	mutex_exit(&kernel_mutex);

	/* update cache last read time */
	now = ut_time_us(NULL);
	cache->last_read = now;

	return(0);
}

/*******************************************************************//**
Returns TRUE if the data in the cache is truncated due to the memory
limit posed by TRX_I_S_MEM_LIMIT.
@return	TRUE if truncated */
UNIV_INTERN
ibool
trx_i_s_cache_is_truncated(
/*=======================*/
	trx_i_s_cache_t*	cache)	/*!< in: cache */
{
	return(cache->is_truncated);
}

/*******************************************************************//**
Initialize INFORMATION SCHEMA trx related cache. */
UNIV_INTERN
void
trx_i_s_cache_init(
/*===============*/
	trx_i_s_cache_t*	cache)	/*!< out: cache to init */
{
	/* The latching is done in the following order:
	acquire trx_i_s_cache_t::rw_lock, X
	acquire kernel_mutex
	release kernel_mutex
	release trx_i_s_cache_t::rw_lock
	acquire trx_i_s_cache_t::rw_lock, S
	release trx_i_s_cache_t::rw_lock */

	rw_lock_create(trx_i_s_cache_lock_key, &cache->rw_lock,
		       SYNC_TRX_I_S_RWLOCK);

	cache->last_read = 0;

	table_cache_init(&cache->innodb_trx, sizeof(i_s_trx_row_t));
	table_cache_init(&cache->innodb_locks, sizeof(i_s_locks_row_t));
	table_cache_init(&cache->innodb_lock_waits,
			 sizeof(i_s_lock_waits_row_t));

	cache->locks_hash = hash_create(LOCKS_HASH_CELLS_NUM);

	cache->storage = ha_storage_create(CACHE_STORAGE_INITIAL_SIZE,
					   CACHE_STORAGE_HASH_CELLS);

	cache->mem_allocd = 0;

	cache->is_truncated = FALSE;
}

/*******************************************************************//**
Free the INFORMATION SCHEMA trx related cache. */
UNIV_INTERN
void
trx_i_s_cache_free(
/*===============*/
	trx_i_s_cache_t*	cache)	/*!< in, own: cache to free */
{
	hash_table_free(cache->locks_hash);
	ha_storage_free(cache->storage);
	table_cache_free(&cache->innodb_trx);
	table_cache_free(&cache->innodb_locks);
	table_cache_free(&cache->innodb_lock_waits);
	memset(cache, 0, sizeof *cache);
}

/*******************************************************************//**
Issue a shared/read lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_start_read(
/*=====================*/
	trx_i_s_cache_t*	cache)	/*!< in: cache */
{
	rw_lock_s_lock(&cache->rw_lock);
}

/*******************************************************************//**
Release a shared/read lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_end_read(
/*===================*/
	trx_i_s_cache_t*	cache)	/*!< in: cache */
{
#ifdef UNIV_SYNC_DEBUG
	ut_a(rw_lock_own(&cache->rw_lock, RW_LOCK_SHARED));
#endif

	rw_lock_s_unlock(&cache->rw_lock);
}

/*******************************************************************//**
Issue an exclusive/write lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_start_write(
/*======================*/
	trx_i_s_cache_t*	cache)	/*!< in: cache */
{
	rw_lock_x_lock(&cache->rw_lock);
}

/*******************************************************************//**
Release an exclusive/write lock on the tables cache. */
UNIV_INTERN
void
trx_i_s_cache_end_write(
/*====================*/
	trx_i_s_cache_t*	cache)	/*!< in: cache */
{
#ifdef UNIV_SYNC_DEBUG
	ut_a(rw_lock_own(&cache->rw_lock, RW_LOCK_EX));
#endif

	rw_lock_x_unlock(&cache->rw_lock);
}

/*******************************************************************//**
Selects a INFORMATION SCHEMA table cache from the whole cache.
@return	table cache */
static
i_s_table_cache_t*
cache_select_table(
/*===============*/
	trx_i_s_cache_t*	cache,	/*!< in: whole cache */
	enum i_s_table		table)	/*!< in: which table */
{
	i_s_table_cache_t*	table_cache;

#ifdef UNIV_SYNC_DEBUG
	ut_a(rw_lock_own(&cache->rw_lock, RW_LOCK_SHARED)
	     || rw_lock_own(&cache->rw_lock, RW_LOCK_EX));
#endif

	switch (table) {
	case I_S_INNODB_TRX:
		table_cache = &cache->innodb_trx;
		break;
	case I_S_INNODB_LOCKS:
		table_cache = &cache->innodb_locks;
		break;
	case I_S_INNODB_LOCK_WAITS:
		table_cache = &cache->innodb_lock_waits;
		break;
	default:
		ut_error;
	}

	return(table_cache);
}

/*******************************************************************//**
Retrieves the number of used rows in the cache for a given
INFORMATION SCHEMA table.
@return	number of rows */
UNIV_INTERN
ulint
trx_i_s_cache_get_rows_used(
/*========================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache */
	enum i_s_table		table)	/*!< in: which table */
{
	i_s_table_cache_t*	table_cache;

	table_cache = cache_select_table(cache, table);

	return(table_cache->rows_used);
}

/*******************************************************************//**
Retrieves the nth row (zero-based) in the cache for a given
INFORMATION SCHEMA table.
@return	row */
UNIV_INTERN
void*
trx_i_s_cache_get_nth_row(
/*======================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache */
	enum i_s_table		table,	/*!< in: which table */
	ulint			n)	/*!< in: row number */
{
	i_s_table_cache_t*	table_cache;
	ulint			i;
	void*			row;

	table_cache = cache_select_table(cache, table);

	ut_a(n < table_cache->rows_used);

	row = NULL;

	for (i = 0; i < MEM_CHUNKS_IN_TABLE_CACHE; i++) {

		if (table_cache->chunks[i].offset
		    + table_cache->chunks[i].rows_allocd > n) {

			row = (char*) table_cache->chunks[i].base
				+ (n - table_cache->chunks[i].offset)
				* table_cache->row_size;
			break;
		}
	}

	ut_a(row != NULL);

	return(row);
}

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
	ulint			lock_id_size)/*!< in: size of the lock id
					buffer */
{
	int	res_len;

	/* please adjust TRX_I_S_LOCK_ID_MAX_LEN if you change this */

	if (row->lock_space != ULINT_UNDEFINED) {
		/* record lock */
		res_len = ut_snprintf(lock_id, lock_id_size,
				      TRX_ID_FMT ":%lu:%lu:%lu",
				      row->lock_trx_id, row->lock_space,
				      row->lock_page, row->lock_rec);
	} else {
		/* table lock */
		res_len = ut_snprintf(lock_id, lock_id_size,
				      TRX_ID_FMT ":%llu",
				      row->lock_trx_id,
				      row->lock_table_id);
	}

	/* the typecast is safe because snprintf(3) never returns
	negative result */
	ut_a(res_len >= 0);
	ut_a((ulint) res_len < lock_id_size);

	return(lock_id);
}
