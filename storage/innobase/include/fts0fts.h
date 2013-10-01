/*****************************************************************************

Copyright (c) 2011, 2013, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file include/fts0fts.h
Full text search header file

Created 2011/09/02 Sunny Bains
***********************************************************************/

#ifndef fts0fts_h
#define fts0fts_h

#include "univ.i"

#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0vec.h"
#include "ut0rbt.h"
#include "ut0wqueue.h"
#include "que0types.h"
#include "ft_global.h"

/** "NULL" value of a document id. */
#define FTS_NULL_DOC_ID			0

/** FTS hidden column that is used to map to and from the row */
#define FTS_DOC_ID_COL_NAME		"FTS_DOC_ID"

/** The name of the index created by FTS */
#define FTS_DOC_ID_INDEX_NAME		"FTS_DOC_ID_INDEX"

#define FTS_DOC_ID_INDEX_NAME_LEN	16

/** Doc ID is a 8 byte value */
#define FTS_DOC_ID_LEN			8

/** The number of fields to sort when we build FT index with
FIC. Three fields are sort: (word, doc_id, position) */
#define FTS_NUM_FIELDS_SORT		3

/** Maximum number of rows in a table, smaller than which, we will
optimize using a 4 byte Doc ID for FIC merge sort to reduce sort size */
#define MAX_DOC_ID_OPT_VAL		1073741824

/** Document id type. */
typedef ib_uint64_t doc_id_t;

/** doc_id_t printf format */
#define FTS_DOC_ID_FORMAT	IB_ID_FMT

/** Convert document id to the InnoDB (BIG ENDIAN) storage format. */
#define fts_write_doc_id(d, s)	mach_write_to_8(d, s)

/** Read a document id to internal format. */
#define fts_read_doc_id(s)	mach_read_from_8(s)

/** Bind the doc id to a variable */
#define fts_bind_doc_id(i, n, v) pars_info_bind_int8_literal(i, n, v)

/** Defines for FTS query mode, they have the same values as
those defined in mysql file ft_global.h */
#define FTS_NL		0
#define FTS_BOOL	1
#define FTS_SORTED	2
#define FTS_EXPAND	4
#define FTS_PROXIMITY	8
#define FTS_PHRASE	16
#define FTS_OPT_RANKING	32

#define FTS_INDEX_TABLE_IND_NAME	"FTS_INDEX_TABLE_IND"

/** Threshold where our optimize thread automatically kicks in */
#define FTS_OPTIMIZE_THRESHOLD		10000000

#define FTS_DOC_ID_MAX_STEP		10000
/** Variable specifying the FTS parallel sort degree */
extern ulong		fts_sort_pll_degree;

/** Variable specifying the number of word to optimize for each optimize table
call */
extern ulong		fts_num_word_optimize;

/** Variable specifying whether we do additional FTS diagnostic printout
in the log */
extern char		fts_enable_diag_print;

/** FTS rank type, which will be between 0 .. 1 inclusive */
typedef float 		fts_rank_t;

/** Type of a row during a transaction. FTS_NOTHING means the row can be
forgotten from the FTS system's POV, FTS_INVALID is an internal value used
to mark invalid states.

NOTE: Do not change the order or value of these, fts_trx_row_get_new_state
depends on them being exactly as they are. */
enum fts_row_state {
	FTS_INSERT = 0,
	FTS_MODIFY,
	FTS_DELETE,
	FTS_NOTHING,
	FTS_INVALID
};

/** The FTS table types. */
enum fts_table_type_t {
	FTS_INDEX_TABLE,		/*!< FTS auxiliary table that is
					specific to a particular FTS index
					on a table */

	FTS_COMMON_TABLE		/*!< FTS auxiliary table that is common
					for all FTS index on a table */
};

struct fts_doc_t;
struct fts_cache_t;
struct fts_token_t;
struct fts_doc_ids_t;
struct fts_index_cache_t;


/** Initialize the "fts_table" for internal query into FTS auxiliary
tables */
#define FTS_INIT_FTS_TABLE(fts_table, m_suffix, m_type, m_table)\
do {								\
	(fts_table)->suffix = m_suffix;				\
        (fts_table)->type = m_type;				\
        (fts_table)->table_id = m_table->id;			\
        (fts_table)->parent = m_table->name;			\
        (fts_table)->table = m_table;				\
} while (0);

#define FTS_INIT_INDEX_TABLE(fts_table, m_suffix, m_type, m_index)\
do {								\
	(fts_table)->suffix = m_suffix;				\
        (fts_table)->type = m_type;				\
        (fts_table)->table_id = m_index->table->id;		\
        (fts_table)->parent = m_index->table->name;		\
        (fts_table)->table = m_index->table;			\
        (fts_table)->index_id = m_index->id;			\
} while (0);

/** Information about changes in a single transaction affecting
the FTS system. */
struct fts_trx_t {
	trx_t*		trx;		/*!< InnoDB transaction */

	ib_vector_t*	savepoints;	/*!< Active savepoints, must have at
					least one element, the implied
					savepoint */
	ib_vector_t*	last_stmt;	/*!< last_stmt */

	mem_heap_t*	heap;		/*!< heap */
};

/** Information required for transaction savepoint handling. */
struct fts_savepoint_t {
	char*		name;		/*!< First entry is always NULL, the
					default instance. Otherwise the name
					of the savepoint */

	ib_rbt_t*	tables;		/*!< Modified FTS tables */
};

/** Information about changed rows in a transaction for a single table. */
struct fts_trx_table_t {
	dict_table_t*	table;		/*!< table */

	fts_trx_t*	fts_trx;	/*!< link to parent */

	ib_rbt_t*	rows;		/*!< rows changed; indexed by doc-id,
					cells are fts_trx_row_t* */

	fts_doc_ids_t*	added_doc_ids;	/*!< list of added doc ids (NULL until
					the first addition) */

					/*!< for adding doc ids */
	que_t*		docs_added_graph;
};

/** Information about one changed row in a transaction. */
struct fts_trx_row_t {
	doc_id_t	doc_id;		/*!< Id of the ins/upd/del document */

	fts_row_state	state;		/*!< state of the row */

	ib_vector_t*	fts_indexes;	/*!< The indexes that are affected */
};

/** List of document ids that were added during a transaction. This
list is passed on to a background 'Add' thread and OPTIMIZE, so it
needs its own memory heap. */
struct fts_doc_ids_t {
	ib_vector_t*	doc_ids;	/*!< document ids (each element is
					of type doc_id_t). */

	ib_alloc_t*	self_heap;	/*!< Allocator used to create an
					instance of this type and the
					doc_ids vector */
};

// FIXME: Get rid of this if possible.
/** Since MySQL's character set support for Unicode is woefully inadequate
(it supports basic operations like isalpha etc. only for 8-bit characters),
we have to implement our own. We use UTF-16 without surrogate processing
as our in-memory format. This typedef is a single such character. */
typedef unsigned short ib_uc_t;

/** An UTF-16 ro UTF-8 string. */
struct fts_string_t {
	byte*		f_str;		/*!< string, not necessary terminated in
					any way */
	ulint		f_len;		/*!< Length of the string in bytes */
	ulint		f_n_char;	/*!< Number of characters */
};

/** Query ranked doc ids. */
struct fts_ranking_t {
	doc_id_t	doc_id;		/*!< Document id */

	fts_rank_t	rank;		/*!< Rank is between 0 .. 1 */

	byte*		words;		/*!< this contains the words
					that were queried
					and found in this document */
	ulint		words_len;	/*!< words len */
};

/** Query result. */
struct fts_result_t {
	ib_rbt_node_t*	current;	/*!< Current element */

	ib_rbt_t*	rankings_by_id;	/*!< RB tree of type fts_ranking_t
					indexed by doc id */
	ib_rbt_t*	rankings_by_rank;/*!< RB tree of type fts_ranking_t
					indexed by rank */
};

/** This is used to generate the FTS auxiliary table name, we need the
table id and the index id to generate the column specific FTS auxiliary
table name. */
struct fts_table_t {
	const char*	parent;		/*!< Parent table name, this is
					required only for the database
					name */

	fts_table_type_t
			type;		/*!< The auxiliary table type */

	table_id_t	table_id;	/*!< The table id */

	index_id_t	index_id;	/*!< The index id */

	const char*	suffix;		/*!< The suffix of the fts auxiliary
					table name, can be NULL, not used
					everywhere (yet) */
	const dict_table_t*
			table;		/*!< Parent table */
	CHARSET_INFO*	charset;	/*!< charset info if it is for FTS
					index auxiliary table */
};

enum	fts_status {
	BG_THREAD_STOP = 1,	 	/*!< TRUE if the FTS background thread
					has finished reading the ADDED table,
					meaning more items can be added to
					the table. */

	BG_THREAD_READY = 2,		/*!< TRUE if the FTS background thread
					is ready */

	ADD_THREAD_STARTED = 4,		/*!< TRUE if the FTS add thread
					has started */

	ADDED_TABLE_SYNCED = 8,		/*!< TRUE if the ADDED table record is
					sync-ed after crash recovery */

	TABLE_DICT_LOCKED = 16		/*!< Set if the table has
					dict_sys->mutex */
};

typedef	enum fts_status	fts_status_t;

/** The state of the FTS sub system. */
struct fts_t {
					/*!< mutex protecting bg_threads* and
					fts_add_wq. */
	ib_mutex_t		bg_threads_mutex;

	ulint		bg_threads;	/*!< number of background threads
					accessing this table */

					/*!< TRUE if background threads running
					should stop themselves */
	ulint		fts_status;	/*!< Status bit regarding fts
					running state */

	ib_wqueue_t*	add_wq;		/*!< Work queue for scheduling jobs
					for the FTS 'Add' thread, or NULL
					if the thread has not yet been
					created. Each work item is a
					fts_trx_doc_ids_t*. */

	fts_cache_t*	cache;		/*!< FTS memory buffer for this table,
					or NULL if the table has no FTS
					index. */

	ulint		doc_col;	/*!< FTS doc id hidden column number
					in the CLUSTERED index. */

	ib_vector_t*	indexes;	/*!< Vector of FTS indexes, this is
					mainly for caching purposes. */
	mem_heap_t*	fts_heap;	/*!< heap for fts_t allocation */
};

struct fts_stopword_t;

/** status bits for fts_stopword_t status field. */
#define STOPWORD_NOT_INIT               0x1
#define STOPWORD_OFF                    0x2
#define STOPWORD_FROM_DEFAULT           0x4
#define STOPWORD_USER_TABLE             0x8

extern const char*	fts_default_stopword[];

/** Variable specifying the maximum FTS cache size for each table */
extern ulong		fts_max_cache_size;

/** Variable specifying the total memory allocated for FTS cache */
extern ulong		fts_max_total_cache_size;

/** Variable specifying the FTS result cache limit for each query */
extern ulong		fts_result_cache_limit;

/** Variable specifying the maximum FTS max token size */
extern ulong		fts_max_token_size;

/** Variable specifying the minimum FTS max token size */
extern ulong		fts_min_token_size;

/** Whether the total memory used for FTS cache is exhausted, and we will
need a sync to free some memory */
extern bool		fts_need_sync;

/** Maximum possible Fulltext word length */
#define FTS_MAX_WORD_LEN		HA_FT_MAXBYTELEN

/** Maximum possible Fulltext word length (in characters) */
#define FTS_MAX_WORD_LEN_IN_CHAR	HA_FT_MAXCHARLEN

/** Variable specifying the table that has Fulltext index to display its
content through information schema table */
extern char*		fts_internal_tbl_name;

#define	fts_que_graph_free(graph)			\
do {							\
	mutex_enter(&dict_sys->mutex);			\
	que_graph_free(graph);				\
	mutex_exit(&dict_sys->mutex);			\
} while (0)

/******************************************************************//**
Create a FTS cache. */
UNIV_INTERN
fts_cache_t*
fts_cache_create(
/*=============*/
	dict_table_t*	table);			/*!< table owns the FTS cache */

/******************************************************************//**
Create a FTS index cache.
@return Index Cache */
UNIV_INTERN
fts_index_cache_t*
fts_cache_index_cache_create(
/*=========================*/
	dict_table_t*	table,			/*!< in: table with FTS index */
	dict_index_t*	index);			/*!< in: FTS index */

/******************************************************************//**
Get the next available document id. This function creates a new
transaction to generate the document id.
@return DB_SUCCESS if OK */
UNIV_INTERN
dberr_t
fts_get_next_doc_id(
/*================*/
	const dict_table_t*	table,	/*!< in: table */
	doc_id_t*		doc_id)	/*!< out: new document id */
	__attribute__((nonnull));
/*********************************************************************//**
Update the next and last Doc ID in the CONFIG table to be the input
"doc_id" value (+ 1). We would do so after each FTS index build or
table truncate */
UNIV_INTERN
void
fts_update_next_doc_id(
/*===================*/
	trx_t*			trx,		/*!< in/out: transaction */
	const dict_table_t*	table,		/*!< in: table */
	const char*		table_name,	/*!< in: table name, or NULL */
	doc_id_t		doc_id)		/*!< in: DOC ID to set */
	__attribute__((nonnull(2)));

/******************************************************************//**
Create a new document id .
@return DB_SUCCESS if all went well else error */
UNIV_INTERN
dberr_t
fts_create_doc_id(
/*==============*/
	dict_table_t*	table,			/*!< in: row is of this
						table. */
	dtuple_t*	row,			/*!< in/out: add doc id
						value to this row. This is the
						current row that is being
						inserted. */
	mem_heap_t*	heap)			/*!< in: heap */
	__attribute__((nonnull));
/******************************************************************//**
Create a new fts_doc_ids_t.
@return new fts_doc_ids_t. */
UNIV_INTERN
fts_doc_ids_t*
fts_doc_ids_create(void);
/*=====================*/

/******************************************************************//**
Free a fts_doc_ids_t. */
UNIV_INTERN
void
fts_doc_ids_free(
/*=============*/
	fts_doc_ids_t*	doc_ids);		/*!< in: doc_ids to free */

/******************************************************************//**
Notify the FTS system about an operation on an FTS-indexed table. */
UNIV_INTERN
void
fts_trx_add_op(
/*===========*/
	trx_t*		trx,			/*!< in: InnoDB transaction */
	dict_table_t*	table,			/*!< in: table */
	doc_id_t	doc_id,			/*!< in: doc id */
	fts_row_state	state,			/*!< in: state of the row */
	ib_vector_t*	fts_indexes)		/*!< in: FTS indexes affected
						(NULL=all) */
	__attribute__((nonnull(1,2)));

/******************************************************************//**
Free an FTS trx. */
UNIV_INTERN
void
fts_trx_free(
/*=========*/
	fts_trx_t*	fts_trx);		/*!< in, own: FTS trx */

/******************************************************************//**
Creates the common ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been
called before this.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_create_common_tables(
/*=====================*/
	trx_t*		trx,			/*!< in: transaction handle */
	const dict_table_t*
			table,			/*!< in: table with one FTS
						index */
	const char*	name,			/*!< in: table name */
	bool		skip_doc_id_index)	/*!< in: Skip index on doc id */
	__attribute__((nonnull, warn_unused_result));
/******************************************************************//**
Wrapper function of fts_create_index_tables_low(), create auxiliary
tables for an FTS index
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_create_index_tables(
/*====================*/
	trx_t*			trx,		/*!< in: transaction handle */
	const dict_index_t*	index)		/*!< in: the FTS index
						instance */
	__attribute__((nonnull, warn_unused_result));
/******************************************************************//**
Creates the column specific ancillary tables needed for supporting an
FTS index on the given table. row_mysql_lock_data_dictionary must have
been called before this.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_create_index_tables_low(
/*========================*/
	trx_t*		trx,			/*!< in: transaction handle */
	const dict_index_t*
			index,			/*!< in: the FTS index
						instance */
	const char*	table_name,		/*!< in: the table name */
	table_id_t	table_id)		/*!< in: the table id */
	__attribute__((nonnull, warn_unused_result));
/******************************************************************//**
Add the FTS document id hidden column. */
UNIV_INTERN
void
fts_add_doc_id_column(
/*==================*/
	dict_table_t*	table,	/*!< in/out: Table with FTS index */
	mem_heap_t*	heap)	/*!< in: temporary memory heap, or NULL */
	__attribute__((nonnull(1)));

/*********************************************************************//**
Drops the ancillary tables needed for supporting an FTS index on the
given table. row_mysql_lock_data_dictionary must have been called before
this.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_drop_tables(
/*============*/
	trx_t*		trx,			/*!< in: transaction */
	dict_table_t*	table)			/*!< in: table has the FTS
						index */
	__attribute__((nonnull));
/******************************************************************//**
The given transaction is about to be committed; do whatever is necessary
from the FTS system's POV.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_commit(
/*=======*/
	trx_t*		trx)			/*!< in: transaction */
	__attribute__((nonnull, warn_unused_result));

/*******************************************************************//**
FTS Query entry point.
@return DB_SUCCESS if successful otherwise error code */
UNIV_INTERN
dberr_t
fts_query(
/*======*/
	trx_t*		trx,			/*!< in: transaction */
	dict_index_t*	index,			/*!< in: FTS index to search */
	uint		flags,			/*!< in: FTS search mode */
	const byte*	query,			/*!< in: FTS query */
	ulint		query_len,		/*!< in: FTS query string len
						in bytes */
	fts_result_t**	result)			/*!< out: query result, to be
						freed by the caller.*/
	__attribute__((nonnull, warn_unused_result));

/******************************************************************//**
Retrieve the FTS Relevance Ranking result for doc with doc_id
@return the relevance ranking value. */
UNIV_INTERN
float
fts_retrieve_ranking(
/*=================*/
	fts_result_t*	result,			/*!< in: FTS result structure */
	doc_id_t	doc_id);		/*!< in: the interested document
						doc_id */

/******************************************************************//**
FTS Query sort result, returned by fts_query() on fts_ranking_t::rank. */
UNIV_INTERN
void
fts_query_sort_result_on_rank(
/*==========================*/
	fts_result_t*	result);		/*!< out: result instance
						to sort.*/

/******************************************************************//**
FTS Query free result, returned by fts_query(). */
UNIV_INTERN
void
fts_query_free_result(
/*==================*/
	fts_result_t*	result);		/*!< in: result instance
						to free.*/

/******************************************************************//**
Extract the doc id from the FTS hidden column. */
UNIV_INTERN
doc_id_t
fts_get_doc_id_from_row(
/*====================*/
	dict_table_t*	table,			/*!< in: table */
	dtuple_t*	row);			/*!< in: row whose FTS doc id we
						want to extract.*/

/******************************************************************//**
Extract the doc id from the FTS hidden column. */
UNIV_INTERN
doc_id_t
fts_get_doc_id_from_rec(
/*====================*/
	dict_table_t*	table,			/*!< in: table */
	const rec_t*	rec,			/*!< in: rec */
	mem_heap_t*	heap);			/*!< in: heap */

/******************************************************************//**
Update the query graph with a new document id.
@return Doc ID used */
UNIV_INTERN
doc_id_t
fts_update_doc_id(
/*==============*/
	dict_table_t*	table,			/*!< in: table */
	upd_field_t*	ufield,			/*!< out: update node */
	doc_id_t*	next_doc_id);		/*!< out: buffer for writing */

/******************************************************************//**
FTS initialize. */
UNIV_INTERN
void
fts_startup(void);
/*==============*/

/******************************************************************//**
Signal FTS threads to initiate shutdown. */
UNIV_INTERN
void
fts_start_shutdown(
/*===============*/
	dict_table_t*	table,			/*!< in: table with FTS
						indexes */
	fts_t*		fts);			/*!< in: fts instance to
						shutdown */

/******************************************************************//**
Wait for FTS threads to shutdown. */
UNIV_INTERN
void
fts_shutdown(
/*=========*/
	dict_table_t*	table,			/*!< in: table with FTS
						indexes */
	fts_t*		fts);			/*!< in: fts instance to
						shutdown */

/******************************************************************//**
Create an instance of fts_t.
@return instance of fts_t */
UNIV_INTERN
fts_t*
fts_create(
/*=======*/
	dict_table_t*	table);			/*!< out: table with FTS
						indexes */

/**********************************************************************//**
Free the FTS resources. */
UNIV_INTERN
void
fts_free(
/*=====*/
	dict_table_t*   table);			/*!< in/out: table with
						FTS indexes */

/*********************************************************************//**
Run OPTIMIZE on the given table.
@return DB_SUCCESS if all OK */
UNIV_INTERN
dberr_t
fts_optimize_table(
/*===============*/
	dict_table_t*	table)			/*!< in: table to optimiza */
	__attribute__((nonnull));

/**********************************************************************//**
Startup the optimize thread and create the work queue. */
UNIV_INTERN
void
fts_optimize_init(void);
/*====================*/

/**********************************************************************//**
Check whether the work queue is initialized.
@return TRUE if optimze queue is initialized. */
UNIV_INTERN
ibool
fts_optimize_is_init(void);
/*======================*/

/****************************************************************//**
Drops index ancillary tables for a FTS index
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_drop_index_tables(
/*==================*/
	trx_t*		trx,			/*!< in: transaction */
	dict_index_t*	index)			/*!< in: Index to drop */
	__attribute__((nonnull, warn_unused_result));

/******************************************************************//**
Remove the table from the OPTIMIZER's list. We do wait for
acknowledgement from the consumer of the message. */
UNIV_INTERN
void
fts_optimize_remove_table(
/*======================*/
	dict_table_t*	table);			/*!< in: table to remove */

/**********************************************************************//**
Signal the optimize thread to prepare for shutdown. */
UNIV_INTERN
void
fts_optimize_start_shutdown(void);
/*==============================*/

/**********************************************************************//**
Inform optimize to clean up. */
UNIV_INTERN
void
fts_optimize_end(void);
/*===================*/

/**********************************************************************//**
Take a FTS savepoint. */
UNIV_INTERN
void
fts_savepoint_take(
/*===============*/
	trx_t*		trx,			/*!< in: transaction */
	const char*	name)			/*!< in: savepoint name */
	__attribute__((nonnull));
/**********************************************************************//**
Refresh last statement savepoint. */
UNIV_INTERN
void
fts_savepoint_laststmt_refresh(
/*===========================*/
	trx_t*		trx)			/*!< in: transaction */
	__attribute__((nonnull));
/**********************************************************************//**
Release the savepoint data identified by  name. */
UNIV_INTERN
void
fts_savepoint_release(
/*==================*/
	trx_t*		trx,			/*!< in: transaction */
	const char*	name);			/*!< in: savepoint name */

/**********************************************************************//**
Free the FTS cache. */
UNIV_INTERN
void
fts_cache_destroy(
/*==============*/
	fts_cache_t*	cache);			/*!< in: cache*/

/*********************************************************************//**
Clear cache. If the shutdown flag is TRUE then the cache can contain
data that needs to be freed. For regular clear as part of normal
working we assume the caller has freed all resources. */
UNIV_INTERN
void
fts_cache_clear(
/*============*/
	fts_cache_t*	cache,			/*!< in: cache */
	ibool		free_words);		/*!< in: TRUE if free
						in memory word cache. */

/*********************************************************************//**
Initialize things in cache. */
UNIV_INTERN
void
fts_cache_init(
/*===========*/
	fts_cache_t*	cache);			/*!< in: cache */

/*********************************************************************//**
Rollback to and including savepoint indentified by name. */
UNIV_INTERN
void
fts_savepoint_rollback(
/*===================*/
	trx_t*		trx,			/*!< in: transaction */
	const char*	name);			/*!< in: savepoint name */

/*********************************************************************//**
Rollback to and including savepoint indentified by name. */
UNIV_INTERN
void
fts_savepoint_rollback_last_stmt(
/*=============================*/
	trx_t*		trx);			/*!< in: transaction */

/***********************************************************************//**
Drop all orphaned FTS auxiliary tables, those that don't have a parent
table or FTS index defined on them. */
UNIV_INTERN
void
fts_drop_orphaned_tables(void);
/*==========================*/

/******************************************************************//**
Since we do a horizontal split on the index table, we need to drop
all the split tables.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_drop_index_split_tables(
/*========================*/
	trx_t*		trx,			/*!< in: transaction */
	dict_index_t*	index)			/*!< in: fts instance */
	__attribute__((nonnull, warn_unused_result));

/****************************************************************//**
Run SYNC on the table, i.e., write out data from the cache to the
FTS auxiliary INDEX table and clear the cache at the end. */
UNIV_INTERN
void
fts_sync_table(
/*===========*/
	dict_table_t*	table)			/*!< in: table */
	__attribute__((nonnull));

/****************************************************************//**
Free the query graph but check whether dict_sys->mutex is already
held */
UNIV_INTERN
void
fts_que_graph_free_check_lock(
/*==========================*/
	fts_table_t*		fts_table,	/*!< in: FTS table */
	const fts_index_cache_t*index_cache,	/*!< in: FTS index cache */
	que_t*			graph);		/*!< in: query graph */

/****************************************************************//**
Create an FTS index cache. */
UNIV_INTERN
CHARSET_INFO*
fts_index_get_charset(
/*==================*/
	dict_index_t*		index);		/*!< in: FTS index */

/*********************************************************************//**
Get the initial Doc ID by consulting the CONFIG table
@return initial Doc ID */
UNIV_INTERN
doc_id_t
fts_init_doc_id(
/*============*/
	const dict_table_t*		table);	/*!< in: table */

/******************************************************************//**
compare two character string according to their charset. */
extern
int
innobase_fts_text_cmp(
/*==================*/
	const void*	cs,			/*!< in: Character set */
	const void*	p1,			/*!< in: key */
	const void*	p2);			/*!< in: node */

/******************************************************************//**
Makes all characters in a string lower case. */
extern
size_t
innobase_fts_casedn_str(
/*====================*/
        CHARSET_INFO*	cs,			/*!< in: Character set */
	char*		src,			/*!< in: string to put in
						lower case */
	size_t		src_len,		/*!< in: input string length */
	char*		dst,			/*!< in: buffer for result
						string */
	size_t		dst_len);		/*!< in: buffer size */


/******************************************************************//**
compare two character string according to their charset. */
extern
int
innobase_fts_text_cmp_prefix(
/*=========================*/
	const void*	cs,			/*!< in: Character set */
	const void*	p1,			/*!< in: key */
	const void*	p2);			/*!< in: node */

/*************************************************************//**
Get the next token from the given string and store it in *token. */
extern
ulint
innobase_mysql_fts_get_token(
/*=========================*/
	CHARSET_INFO*	charset,		/*!< in: Character set */
	const byte*	start,			/*!< in: start of text */
	const byte*	end,			/*!< in: one character past
						end of text */
	fts_string_t*	token,			/*!< out: token's text */
	ulint*		offset);		/*!< out: offset to token,
						measured as characters from
						'start' */

/*********************************************************************//**
Fetch COUNT(*) from specified table.
@return the number of rows in the table */
UNIV_INTERN
ulint
fts_get_rows_count(
/*===============*/
	fts_table_t*	fts_table);		/*!< in: fts table to read */

/*************************************************************//**
Get maximum Doc ID in a table if index "FTS_DOC_ID_INDEX" exists
@return max Doc ID or 0 if index "FTS_DOC_ID_INDEX" does not exist */
UNIV_INTERN
doc_id_t
fts_get_max_doc_id(
/*===============*/
	dict_table_t*	table);			/*!< in: user table */

/******************************************************************//**
Check whether user supplied stopword table exists and is of
the right format.
@return the stopword column charset if qualifies */
UNIV_INTERN
CHARSET_INFO*
fts_valid_stopword_table(
/*=====================*/
	const char*	stopword_table_name);	/*!< in: Stopword table
						name */
/****************************************************************//**
This function loads specified stopword into FTS cache
@return TRUE if success */
UNIV_INTERN
ibool
fts_load_stopword(
/*==============*/
	const dict_table_t*
			table,			/*!< in: Table with FTS */
	trx_t*		trx,			/*!< in: Transaction */
	const char*	global_stopword_table,	/*!< in: Global stopword table
						name */
	const char*	session_stopword_table,	/*!< in: Session stopword table
						name */
	ibool		stopword_is_on,		/*!< in: Whether stopword
						option is turned on/off */
	ibool		reload);		/*!< in: Whether it is during
						reload of FTS table */

/****************************************************************//**
Create the vector of fts_get_doc_t instances.
@return vector of fts_get_doc_t instances */
UNIV_INTERN
ib_vector_t*
fts_get_docs_create(
/*================*/
	fts_cache_t*	cache);			/*!< in: fts cache */

/****************************************************************//**
Read the rows from the FTS index
@return DB_SUCCESS if OK */
UNIV_INTERN
dberr_t
fts_table_fetch_doc_ids(
/*====================*/
	trx_t*		trx,			/*!< in: transaction */
	fts_table_t*	fts_table,		/*!< in: aux table */
	fts_doc_ids_t*	doc_ids);		/*!< in: For collecting
						doc ids */
/****************************************************************//**
This function brings FTS index in sync when FTS index is first
used. There are documents that have not yet sync-ed to auxiliary
tables from last server abnormally shutdown, we will need to bring
such document into FTS cache before any further operations
@return TRUE if all OK */
UNIV_INTERN
ibool
fts_init_index(
/*===========*/
	dict_table_t*	table,			/*!< in: Table with FTS */
	ibool		has_cache_lock);	/*!< in: Whether we already
						have cache lock */
/*******************************************************************//**
Add a newly create index in FTS cache */
UNIV_INTERN
void
fts_add_index(
/*==========*/
	dict_index_t*	index,			/*!< FTS index to be added */
	dict_table_t*	table);			/*!< table */

/*******************************************************************//**
Drop auxiliary tables related to an FTS index
@return DB_SUCCESS or error number */
UNIV_INTERN
dberr_t
fts_drop_index(
/*===========*/
	dict_table_t*	table,	/*!< in: Table where indexes are dropped */
	dict_index_t*	index,	/*!< in: Index to be dropped */
	trx_t*		trx)	/*!< in: Transaction for the drop */
	__attribute__((nonnull));

/****************************************************************//**
Rename auxiliary tables for all fts index for a table
@return DB_SUCCESS or error code */

dberr_t
fts_rename_aux_tables(
/*==================*/
	dict_table_t*	table,		/*!< in: user Table */
	const char*	new_name,	/*!< in: new table name */
	trx_t*		trx);		/*!< in: transaction */

/*******************************************************************//**
Check indexes in the fts->indexes is also present in index cache and
table->indexes list
@return TRUE if all indexes match */
UNIV_INTERN
ibool
fts_check_cached_index(
/*===================*/
	dict_table_t*	table);  /*!< in: Table where indexes are dropped */
#endif /*!< fts0fts.h */

