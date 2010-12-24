/******************************************************
Full Text Search functionality.

(c) 2006 Innobase Oy

Created 2006-02-15 Osku Salerma
*******************************************************/

#ifndef INNODB_FTS0FTS_H
#define INNODB_FTS0FTS_H

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

/* FTS hidden column that is used to map to and from the row */
#define FTS_DOC_ID_COL_NAME		"FTS_DOC_ID"

/* The name of the index created by FTS */
#define FTS_DOC_ID_INDEX_NAME		"FTS_DOC_ID_INDEX"

#define FTS_DOC_ID_INDEX_NAME_LEN	16

/* Doc ID is a 8 byte value */
#define FTS_DOC_ID_LEN			8

/* The number of fields to sort when we build FT index with
FIC. Three fields are sort: (word, doc_id, position) */
#define FTS_NUM_FIELDS_SORT		3

/* Minimum token size, when less than this size, we will
exclude it from FT index */
#define FTS_MIN_TOKEN_SIZE		3

/* Document id type. */
typedef ib_uint64_t doc_id_t;

/* doc_id_t printf format */
#define FTS_DOC_ID_FORMAT	"%llu"

/* Convert document id to the InnoDB (BIG ENDIAN) storage format. */
#define fts_write_doc_id(d, s)	mach_write_to_8(d, s)

/* Read a document id to internal format. */
#define fts_read_doc_id(s)	mach_read_from_8(s)

/* Bind the doc id to a variable */
#define fts_bind_doc_id(i, n, v) pars_info_bind_int8_literal(i, n, v)

/* Defines for FTS query mode, they have the same values as
those defined in mysql file ft_global.h */
#define FTS_NL		0
#define FTS_BOOL	1
#define FTS_SORTED	2
#define FTS_EXPAND	4
#define FTS_PROXIMITY	8
#define FTS_PHRASE	16

/* FTS rank type, which will be between 0 .. 1 inclusive */
typedef float fts_rank_t;

/* Type of a row during a transaction. FTS_NOTHING means the row can be
forgotten from the FTS system's POV, FTS_INVALID is an internal value used
to mark invalid states.

NOTE: Do not change the order or value of these, fts_trx_row_get_new_state
depends on them being exactly as they are. */
typedef enum {
	FTS_INSERT = 0,
	FTS_MODIFY,
	FTS_DELETE,
	FTS_NOTHING,
	FTS_INVALID
} fts_row_state;

/* The FTS table types. */
enum fts_table_type_enum {
	FTS_INDEX_TABLE,		/* FTS auxiliary table that is specific
					to a particular FTS index on a table */

	FTS_COMMON_TABLE		/* FTS auxiliary table that is common
					for all FTS index on a table */
};

typedef struct fts_struct fts_t;
typedef struct fts_doc_struct fts_doc_t;
typedef struct fts_trx_struct fts_trx_t;
typedef struct fts_table_struct fts_table_t;
typedef struct fts_cache_struct fts_cache_t;
typedef struct fts_token_struct fts_token_t;
typedef struct fts_string_struct fts_string_t;
typedef	struct fts_result_struct fts_result_t;
typedef struct fts_ranking_struct fts_ranking_t;
typedef struct fts_trx_row_struct fts_trx_row_t;
typedef struct fts_doc_ids_struct fts_doc_ids_t;
typedef enum fts_table_type_enum fts_table_type_t;
typedef struct fts_trx_table_struct fts_trx_table_t;
typedef	struct fts_savepoint_struct fts_savepoint_t;
typedef struct fts_index_cache_struct fts_index_cache_t;

/********************************************************************
Create a FTS cache. */

fts_cache_t*
fts_cache_create(
/*=============*/
	dict_table_t*	table);		/*!< table owns the FTS cache */
/********************************************************************
Create a FTS index cache. */

void
fts_cache_index_cache_create(
/*=========================*/
	dict_table_t*	table,		/* in: table with FTS index */
	dict_index_t*	index);		/* in: FTS index */
/********************************************************************
Get the next available document id. This function creates a new
transaction to generate the document id. */

ulint
fts_get_next_doc_id(
/*================*/
					/* out: DB_SUCCESS if OK */
	dict_table_t*	table,		/* in: table */
	doc_id_t*	doc_id);	/* out: new document id */
/********************************************************************
Update the last document id. This function could create a new
transaction to update the last document id. */

ulint
fts_update_last_doc_id(
/*===================*/
					/* out: DB_SUCCESS if OK */
	dict_table_t*	table,		/* in: table */
	doc_id_t	doc_id,		/* in: last document id */
	trx_t*		trx);		/* in: update trx */
/********************************************************************
Create a new document id .*/

ulint
fts_create_doc_id(
/*==============*/
					/* out: DB_SUCCESS if all went well
					else error */
	dict_table_t*	table,		/* in: row is of this table. */
	dtuple_t*	row,		/* in/out: add doc id value to this
					row. This is the current row that is
					being inserted. */
	mem_heap_t*	heap);		/* in: heap */
/********************************************************************
Create a new fts_doc_ids_t. */

fts_doc_ids_t*
fts_doc_ids_create(void);
/*=====================*/
					/* out, own: new fts_doc_ids_t */
/********************************************************************
Free a fts_doc_ids_t. */

void
fts_doc_ids_free(
/*=============*/
	fts_doc_ids_t*	doc_ids);	/* in: doc_ids to free */
/********************************************************************
Notify the FTS system about an operation on an FTS-indexed table. */

void
fts_trx_add_op(
/*===========*/
	trx_t*		trx,		/* in: InnoDB transaction */
	dict_table_t*	table,		/* in: table */
	doc_id_t	doc_id,		/* in: doc id */
	fts_row_state	state,		/* in: state of the row */
	ib_vector_t*	indexes);	/* in: FTS indexes affected */
/********************************************************************
Free an FTS trx. */

void
fts_trx_free(
/*=========*/
	fts_trx_t*	fts_trx);	/* in, own: FTS trx */
/********************************************************************
Creates the common ancillary tables needed for supporting an FTS index
on the given table. row_mysql_lock_data_dictionary must have been
called before this. */

ulint
fts_create_common_tables(
/*=====================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction handle */
	const dict_table_t*
			table,		/* in: table with one FTS index */
	const char*	name,		/* in: table name */
	ibool		skip_doc_id_index);
					/* in: Skip index on doc id */
/********************************************************************
Creates the column specific ancillary tables needed for supporting an
FTS index on the given table. row_mysql_lock_data_dictionary must have
been called before this. */

ulint
fts_create_index_tables(
/*====================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction handle */
	const dict_index_t*
			index);		/* in: the FTS index instance */
/********************************************************************
Add the FTS document id hidden column. */

void
fts_add_doc_id_column(
/*==================*/
	dict_table_t*	table);		/* in/out: Table with FTS index */
/********************************************************************
Drops the ancillary tables needed for supporting an FTS index on the
given table. row_mysql_lock_data_dictionary must have been called before
this. */

ulint
fts_drop_tables(
/*============*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction handle */
	fts_t*		fts,		/* in: FTS instance */
	fts_table_t*	fts_table);	/* in: fts common table id */
/********************************************************************
The given transaction is about to be committed; do whatever is necessary
from the FTS system's POV. */

ulint
fts_commit(
/*=======*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx);		/* in: transaction */
/*******************************************************************//**
FTS Query entry point.
@return DB_SUCCESS if successful otherwise error code */
UNIV_INTERN
ulint
fts_query(
/*======*/
	trx_t*		trx,		/*!< in: transaction */
	dict_index_t*	index,		/*!< in: FTS index to search */
	uint		flags,		/*!< in: FTS search mode */
	const byte*	query,		/*!< in: FTS query */
	ulint		query_len,	/*!< in: FTS query string len
					in bytes */
	fts_result_t**	result);	/*!< out: query result, to be freed
					by the caller.*/
/********************************************************************
Retrieve the FTS Relevance Ranking result for doc with doc_id
@return the relevance ranking value. */
float
fts_retrieve_ranking(
/*=================*/
	fts_result_t*	result,		/* in: FTS result structure */
	doc_id_t	doc_id);	/* in: the interested document
					doc_id */

/********************************************************************
FTS Query sort result, returned by fts_query() on fts_ranking_t::rank. */

void
fts_query_sort_result_on_rank(
/*==========================*/
	fts_result_t*	result);	/* out: result instance to sort.*/
/********************************************************************
FTS Query free result, returned by fts_query(). */

void
fts_query_free_result(
/*==================*/
	fts_result_t*	result);	/* in: result instance to free.*/
/********************************************************************
Start function for the background 'Add' threads. */

os_thread_ret_t
fts_add_thread(
/*===========*/
					/* out: a dummy parameter */
	void*		arg);		/* in: dict_table_t* */
/********************************************************************
Extract the doc id from the FTS hidden column. */

doc_id_t
fts_get_doc_id_from_row(
/*====================*/
	dict_table_t*	table,		/* in: table */
	dtuple_t*	row);		/* in: row whose FTS doc id we
					want to extract.*/
/********************************************************************
Extract the doc id from the FTS hidden column. */

doc_id_t
fts_get_doc_id_from_rec(
/*====================*/
	dict_table_t*	table,		/* in: table */
	const rec_t*	rec,		/* in: rec */
	mem_heap_t*	heap);		/* in: heap */
/********************************************************************
Update the query graph with a new document id. */

ulint
fts_update_doc_id(
/*==============*/
	dict_table_t*	table,		/* in: table */
	upd_field_t*	ufield,		/* out: update node */
	doc_id_t*	next_doc_id);	/* out: buffer for writing */
/********************************************************************
FTS initialize. */

void
fts_startup(void);
/*==============*/
/********************************************************************
Signal FTS threads to initiate shutdown. */

void
fts_start_shutdown(
/*===============*/
	dict_table_t*	table,		/* in: table with FTS indexes */
	fts_t*		fts);		/* in: fts instance to shutdown */
/********************************************************************
Wait for FTS threads to shutdown. */

void
fts_shutdown(
/*=========*/
	dict_table_t*	table,		/* in: table with FTS indexes */
	fts_t*		fts);		/* in: fts instance to shutdown */
/********************************************************************
Create an instance of fts_t. */

fts_t*
fts_create(
/*=======*/
					/* out: instance of fts_t */
	dict_table_t*	table);		/* out: table with FTS indexes */
/************************************************************************
Free the FTS resources. */

void
fts_free(
/*=====*/
	fts_t*		fts);		/* out: fts_t instance */
/********************************************************************
Startup the optimize thread and create the work queue. */

void
fts_optimize_init(void);
/*====================*/
/********************************************************************
Signal the optimize thread to prepare for shutdown. */

void
fts_optimize_start_shutdown(void);
/*==============================*/

/********************************************************************
Inform optimize to clean up. */

void
fts_optimize_end(void);
/*===================*/

/********************************************************************
Take a FTS savepoint. */

void
fts_savepoint_take(
/*===============*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const char*	name);		/* in: savepoint name */

/********************************************************************
Release the savepoint data identified by  name. */

void
fts_savepoint_release(
/*==================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const char*	name);		/* in: savepoint name */

/********************************************************************
Rollback to and including savepoint indentified by name. */

void
fts_savepoint_rollback(
/*===================*/
					/* out: DB_SUCCESS or error code */
	trx_t*		trx,		/* in: transaction */
	const char*	name);		/* in: savepoint name */

/*************************************************************************
Drop all orphaned FTS auxiliary tables, those that don't have a parent
table or FTS index defined on them. */

void
fts_drop_orphaned_tables(void);
/*==========================*/

/* Information about changes in a single transaction affecting
the FTS system. */
struct fts_trx_struct {
	trx_t*		trx;		/* InnoDB transaction */

	ib_vector_t*	savepoints;	/* Active savepoints, must have at
					least one element, the implied
					savepoint */

	mem_heap_t*	heap;		/* heap */
};

/* Information required for transaction savepoint handling. */
struct fts_savepoint_struct {
	char*		name;		/* First entry is always NULL, the
					default instance. Otherwise the name
					of the savepoint */

	ib_rbt_t*	tables;		/* Modified FTS tables */
};

/* Information about changed rows in a transaction for a single table. */
struct fts_trx_table_struct {
	dict_table_t*	table;		/* table */

	fts_trx_t*	fts_trx;	/* link to parent */

	ib_rbt_t*	rows;		/* rows changed; indexed by doc-id,
					cells are fts_trx_row_t* */

	fts_doc_ids_t*	added_doc_ids;	/* list of added doc ids (NULL until
					the first addition) */

					/* for adding doc ids */
	que_t*		docs_added_graph;
};

/* Information about one changed row in a transaction. */
struct fts_trx_row_struct {
	doc_id_t	doc_id;		/* Id of the ins/upd/del document */

	fts_row_state	state;		/* state of the row */

	ib_vector_t*	fts_indexes;	/* The indexes that are affected */
};

/* List of document ids that were added during a transaction. This
list is passed on to a background 'Add' thread and OPTIMIZE, so it
needs its own memory heap. */
struct fts_doc_ids_struct {
	ib_vector_t*	doc_ids;	/* document ids (each element is
					of type doc_id_t). */

	ib_alloc_t*	self_heap;	/* Allocator used to create an
					instance of this type and the
					doc_ids vector */
};

// FIXME: Get rid of this if possible.
/* Since MySQL's character set support for Unicode is woefully inadequate
(it supports basic operations like isalpha etc. only for 8-bit characters),
we have to implement our own. We use UTF-16 without surrogate processing
as our in-memory format. This typedef is a single such character. */
typedef unsigned short ib_uc_t;

/* An UTF-16 ro UTF-8 string. */
struct fts_string_struct {
	byte*		utf8;		/* UTF-8 string, not terminated in
					any way */

	ulint		len;		/* Length of the string in bytes
					for UTF-8 strings */
};

/* Query ranked doc ids. */
struct fts_ranking_struct {
	doc_id_t	doc_id;		/* Document id */

	fts_rank_t	rank;		/* Rank is between 0 .. 1 */

	ib_rbt_t*	words;		/* RB Tree of type byte*, this contains
					the words that were queried and found
					in this document */
};

/* Query result. */
struct fts_result_struct {
	ib_rbt_node_t*	current;	/* Current element */

	ib_rbt_t*	rankings;	/* RB tree of type fts_ranking_t
					indexed by doc id */
};

/* This is used to generate the FTS auxiliary table name, we need the
table id and the index id to generate the column specific FTS auxiliary
table name. */
struct fts_table_struct {
	const char*	parent;		/* Parent table name, this is
					required only for the database
					name */

	fts_table_type_t
			type;		/* The auxiliary table type */

	table_id_t	table_id;	/* The table id */

	index_id_t	index_id;	/* The index id */

	const char*	suffix;		/* The suffix of the fts auxiliary
					table name, can be NULL, not used
					everywhere (yet) */
};

enum	fts_status {
	BG_THREAD_STOP = 1,	 /* TRUE if the FTS background thread
				has finished reading the ADDED table,
				meaning more items can be added to
				the table. */
	BG_THREAD_READY = 2,	/* TRUE if the FTS background thread
				is ready */
	ADD_THREAD_STARTED = 4,	/* TRUE if the FTS add thread started */
	ADDED_TABLE_SYNCED = 8,	/* TRUE if the ADDED table record is sync-ed
				after crash recovery */
};

typedef	enum fts_status	fts_status_t;
	
/* The state of the FTS sub system. */
struct fts_struct {
					/* mutex protecting bg_threads* and
					fts_add_wq. */
	mutex_t		bg_threads_mutex;

	ulint		bg_threads;	/* number of background threads
					accessing this table */

					/* TRUE if background threads running
					should stop themselves */
	ulint		fts_status;	/* Status bit regarding fts
					running state */

	ib_wqueue_t*	add_wq;		/* Work queue for scheduling jobs
					for the FTS 'Add' thread, or NULL
					if the thread has not yet been
					created. Each work item is a
					fts_trx_doc_ids_t*. */

	fts_cache_t*	cache;		/* FTS memory buffer for this table,
					or NULL if the table has no FTS
					index. */

	ulint		doc_col;	/* FTS doc id hidden column number
					in the CLUSTERED index. */

	ib_vector_t*	indexes;	/* Vector of FTS indexes, this is
					mainly for caching purposes. */
};

#endif /* INNOBASE_FTS0FTS_H */
