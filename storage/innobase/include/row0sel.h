/******************************************************
Select

(c) 1997 Innobase Oy

Created 12/19/1997 Heikki Tuuri
*******************************************************/

#ifndef row0sel_h
#define row0sel_h

#include "univ.i"
#include "data0data.h"
#include "que0types.h"
#include "dict0types.h"
#include "trx0types.h"
#include "row0types.h"
#include "que0types.h"
#include "pars0sym.h"
#include "btr0pcur.h"
#include "read0read.h"
#include "row0mysql.h"

/*************************************************************************
Creates a select node struct. */

sel_node_t*
sel_node_create(
/*============*/
				/* out, own: select node struct */
	mem_heap_t*	heap);	/* in: memory heap where created */
/*************************************************************************
Frees the memory private to a select node when a query graph is freed,
does not free the heap where the node was originally created. */

void
sel_node_free_private(
/*==================*/
	sel_node_t*	node);	/* in: select node struct */
/*************************************************************************
Frees a prefetch buffer for a column, including the dynamically allocated
memory for data stored there. */

void
sel_col_prefetch_buf_free(
/*======================*/
	sel_buf_t*	prefetch_buf);	/* in, own: prefetch buffer */
/*************************************************************************
Gets the plan node for the nth table in a join. */
UNIV_INLINE
plan_t*
sel_node_get_nth_plan(
/*==================*/
	sel_node_t*	node,
	ulint		i);
/**************************************************************************
Performs a select step. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
row_sel_step(
/*=========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of an open or close cursor statement node. */
UNIV_INLINE
que_thr_t*
open_step(
/*======*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs a fetch for a cursor. */

que_thr_t*
fetch_step(
/*=======*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/********************************************************************
Sample callback function for fetch that prints each row.*/

void*
row_fetch_print(
/*============*/
				/* out: always returns non-NULL */
	void*	row,		/* in:  sel_node_t* */
	void*	user_arg);	/* in:  not used */
/********************************************************************
Callback function for fetch that stores an unsigned 4 byte integer to the
location pointed. The column's type must be DATA_INT, DATA_UNSIGNED, length
= 4. */

void*
row_fetch_store_uint4(
/*==================*/
				/* out: always returns NULL */
	void*	row,		/* in:  sel_node_t* */
	void*	user_arg);	/* in:  data pointer */
/***************************************************************
Prints a row in a select result. */

que_thr_t*
row_printf_step(
/*============*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/********************************************************************
Converts a key value stored in MySQL format to an Innobase dtuple. The last
field of the key value may be just a prefix of a fixed length field: hence
the parameter key_len. But currently we do not allow search keys where the
last field is only a prefix of the full key field len and print a warning if
such appears. */

void
row_sel_convert_mysql_key_to_innobase(
/*==================================*/
	dtuple_t*	tuple,		/* in: tuple where to build;
					NOTE: we assume that the type info
					in the tuple is already according
					to index! */
	byte*		buf,		/* in: buffer to use in field
					conversions */
	ulint		buf_len,	/* in: buffer length */
	dict_index_t*	index,		/* in: index of the key value */
	byte*		key_ptr,	/* in: MySQL key value */
	ulint		key_len,	/* in: MySQL key value length */
	trx_t*		trx);		/* in: transaction */
/************************************************************************
Searches for rows in the database. This is used in the interface to
MySQL. This function opens a cursor, and also implements fetch next
and fetch prev. NOTE that if we do a search with a full key value
from a unique index (ROW_SEL_EXACT), then we will not store the cursor
position and fetch next or fetch prev must not be tried to the cursor! */

ulint
row_search_for_mysql(
/*=================*/
					/* out: DB_SUCCESS,
					DB_RECORD_NOT_FOUND,
					DB_END_OF_INDEX, DB_DEADLOCK,
					DB_LOCK_TABLE_FULL,
					or DB_TOO_BIG_RECORD */
	byte*		buf,		/* in/out: buffer for the fetched
					row in the MySQL format */
	ulint		mode,		/* in: search mode PAGE_CUR_L, ... */
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct for the
					table handle; this contains the info
					of search_tuple, index; if search
					tuple contains 0 fields then we
					position the cursor at the start or
					the end of the index, depending on
					'mode' */
	ulint		match_mode,	/* in: 0 or ROW_SEL_EXACT or
					ROW_SEL_EXACT_PREFIX */
	ulint		direction);	/* in: 0 or ROW_SEL_NEXT or
					ROW_SEL_PREV; NOTE: if this is != 0,
					then prebuilt must have a pcur
					with stored position! In opening of a
					cursor 'direction' should be 0. */
/***********************************************************************
Checks if MySQL at the moment is allowed for this table to retrieve a
consistent read result, or store it to the query cache. */

ibool
row_search_check_if_query_cache_permitted(
/*======================================*/
					/* out: TRUE if storing or retrieving
					from the query cache is permitted */
	trx_t*		trx,		/* in: transaction object */
	const char*	norm_name);	/* in: concatenation of database name,
					'/' char, table name */


/* A structure for caching column values for prefetched rows */
struct sel_buf_struct{
	byte*		data;	/* data, or NULL; if not NULL, this field
				has allocated memory which must be explicitly
				freed; can be != NULL even when len is
				UNIV_SQL_NULL */
	ulint		len;	/* data length or UNIV_SQL_NULL */
	ulint		val_buf_size;
				/* size of memory buffer allocated for data:
				this can be more than len; this is defined
				when data != NULL */
};

struct plan_struct{
	dict_table_t*	table;		/* table struct in the dictionary
					cache */
	dict_index_t*	index;		/* table index used in the search */
	btr_pcur_t	pcur;		/* persistent cursor used to search
					the index */
	ibool		asc;		/* TRUE if cursor traveling upwards */
	ibool		pcur_is_open;	/* TRUE if pcur has been positioned
					and we can try to fetch new rows */
	ibool		cursor_at_end;	/* TRUE if the cursor is open but
					we know that there are no more
					qualifying rows left to retrieve from
					the index tree; NOTE though, that
					there may still be unprocessed rows in
					the prefetch stack; always FALSE when
					pcur_is_open is FALSE */
	ibool		stored_cursor_rec_processed;
					/* TRUE if the pcur position has been
					stored and the record it is positioned
					on has already been processed */
	que_node_t**	tuple_exps;	/* array of expressions which are used
					to calculate the field values in the
					search tuple: there is one expression
					for each field in the search tuple */
	dtuple_t*	tuple;		/* search tuple */
	ulint		mode;		/* search mode: PAGE_CUR_G, ... */
	ulint		n_exact_match;	/* number of first fields in the search
					tuple which must be exactly matched */
	ibool		unique_search;	/* TRUE if we are searching an
					index record with a unique key */
	ulint		n_rows_fetched;	/* number of rows fetched using pcur
					after it was opened */
	ulint		n_rows_prefetched;/* number of prefetched rows cached
					for fetch: fetching several rows in
					the same mtr saves CPU time */
	ulint		first_prefetched;/* index of the first cached row in
					select buffer arrays for each column */
	ibool		no_prefetch;	/* no prefetch for this table */
	sym_node_list_t	columns;	/* symbol table nodes for the columns
					to retrieve from the table */
	UT_LIST_BASE_NODE_T(func_node_t)
			end_conds;	/* conditions which determine the
					fetch limit of the index segment we
					have to look at: when one of these
					fails, the result set has been
					exhausted for the cursor in this
					index; these conditions are normalized
					so that in a comparison the column
					for this table is the first argument */
	UT_LIST_BASE_NODE_T(func_node_t)
			other_conds;	/* the rest of search conditions we can
					test at this table in a join */
	ibool		must_get_clust;	/* TRUE if index is a non-clustered
					index and we must also fetch the
					clustered index record; this is the
					case if the non-clustered record does
					not contain all the needed columns, or
					if this is a single-table explicit
					cursor, or a searched update or
					delete */
	ulint*		clust_map;	/* map telling how clust_ref is built
					from the fields of a non-clustered
					record */
	dtuple_t*	clust_ref;	/* the reference to the clustered
					index entry is built here if index is
					a non-clustered index */
	btr_pcur_t	clust_pcur;	/* if index is non-clustered, we use
					this pcur to search the clustered
					index */
	mem_heap_t*	old_vers_heap;	/* memory heap used in building an old
					version of a row, or NULL */
};

struct sel_node_struct{
	que_common_t	common;		/* node type: QUE_NODE_SELECT */
	ulint		state;		/* node state */
	que_node_t*	select_list;	/* select list */
	sym_node_t*	into_list;	/* variables list or NULL */
	sym_node_t*	table_list;	/* table list */
	ibool		asc;		/* TRUE if the rows should be fetched
					in an ascending order */
	ibool		set_x_locks;	/* TRUE if the cursor is for update or
					delete, which means that a row x-lock
					should be placed on the cursor row */
	ibool		select_will_do_update;
					/* TRUE if the select is for a searched
					update which can be performed in-place:
					in this case the select will take care
					of the update */
	ulint		latch_mode;	/* BTR_SEARCH_LEAF, or BTR_MODIFY_LEAF
					if select_will_do_update is TRUE */
	ulint		row_lock_mode;	/* LOCK_X or LOCK_S */
	ulint		n_tables;	/* number of tables */
	ulint		fetch_table;	/* number of the next table to access
					in the join */
	plan_t*		plans;		/* array of n_tables many plan nodes
					containing the search plan and the
					search data structures */
	que_node_t*	search_cond;	/* search condition */
	read_view_t*	read_view;	/* if the query is a non-locking
					consistent read, its read view is
					placed here, otherwise NULL */
	ibool		consistent_read;/* TRUE if the select is a consistent,
					non-locking read */
	order_node_t*	order_by;	/* order by column definition, or
					NULL */
	ibool		is_aggregate;	/* TRUE if the select list consists of
					aggregate functions */
	ibool		aggregate_already_fetched;
					/* TRUE if the aggregate row has
					already been fetched for the current
					cursor */
	ibool		can_get_updated;/* this is TRUE if the select
					is in a single-table explicit
					cursor which can get updated
					within the stored procedure,
					or in a searched update or
					delete; NOTE that to determine
					of an explicit cursor if it
					can get updated, the parser
					checks from a stored procedure
					if it contains positioned
					update or delete statements */
	sym_node_t*	explicit_cursor;/* not NULL if an explicit cursor */
	UT_LIST_BASE_NODE_T(sym_node_t)
			copy_variables; /* variables whose values we have to
					copy when an explicit cursor is opened,
					so that they do not change between
					fetches */
};

/* Select node states */
#define	SEL_NODE_CLOSED		0	/* it is a declared cursor which is not
					currently open */
#define SEL_NODE_OPEN		1	/* intention locks not yet set on
					tables */
#define SEL_NODE_FETCH		2	/* intention locks have been set */
#define SEL_NODE_NO_MORE_ROWS	3	/* cursor has reached the result set
					end */

/* Fetch statement node */
struct fetch_node_struct{
	que_common_t	common;		/* type: QUE_NODE_FETCH */
	sel_node_t*	cursor_def;	/* cursor definition */
	sym_node_t*	into_list;	/* variables to set */

	pars_user_func_t*
			func;		/* User callback function or NULL.
					The first argument to the function
					is a sel_node_t*, containing the
					results of the SELECT operation for
					one row. If the function returns
					NULL, it is not interested in
					further rows and the cursor is
					modified so (cursor % NOTFOUND) is
					true. If it returns not-NULL,
					continue normally. See
					row_fetch_print() for an example
					(and a useful debugging tool). */
};

/* Open or close cursor statement node */
struct open_node_struct{
	que_common_t	common;		/* type: QUE_NODE_OPEN */
	ulint		op_type;	/* ROW_SEL_OPEN_CURSOR or
					ROW_SEL_CLOSE_CURSOR */
	sel_node_t*	cursor_def;	/* cursor definition */
};

/* Row printf statement node */
struct row_printf_node_struct{
	que_common_t	common;		/* type: QUE_NODE_ROW_PRINTF */
	sel_node_t*	sel_node;	/* select */
};

#define ROW_SEL_OPEN_CURSOR	0
#define ROW_SEL_CLOSE_CURSOR	1

/* Flags for the MySQL interface */
#define ROW_SEL_NEXT		1
#define ROW_SEL_PREV		2

#define ROW_SEL_EXACT		1	/* search using a complete key value */
#define ROW_SEL_EXACT_PREFIX	2	/* search using a key prefix which
					must match to rows: the prefix may
					contain an incomplete field (the
					last field in prefix may be just
					a prefix of a fixed length column) */

#ifndef UNIV_NONINL
#include "row0sel.ic"
#endif

#endif
